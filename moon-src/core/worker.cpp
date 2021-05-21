#include "worker.h"
#include "common/time.hpp"
#include "common/string.hpp"
#include "common/hash.hpp"
#include "message.hpp"
#include "service.hpp"
#include "server.h"

namespace moon
{
    worker::worker(server* srv, uint32_t id)
        : workerid_(id)
        , server_(srv)
        , io_ctx_(1)
        , work_(asio::make_work_guard(io_ctx_))
    {
    }

    worker::~worker()
    {
        wait();
    }

    void worker::run()
    {
        socket_ = std::make_unique<moon::socket>(server_, this, io_ctx_);

        thread_ = std::thread([this]() {
            CONSOLE_INFO(server_->logger(), "WORKER-%u START", workerid_);
            io_ctx_.run();
            socket_->close_all();
            services_.clear();
            CONSOLE_INFO(server_->logger(), "WORKER-%u STOP", workerid_);
        });

    }

    void worker::stop()
    {
        asio::post(io_ctx_, [this] {
            message msg;
            msg.set_type(PTYPE_SHUTDOWN);
            for (auto& it : services_)
            {
                it.second->dispatch(&msg);
            }
         });
    }

    void worker::wait()
    {
        io_ctx_.stop();
        if (thread_.joinable())
        {
            thread_.join();
        }
    }

    void worker::new_service(std::string service_type, service_conf conf, uint32_t creatorid, int32_t sessionid)
    {
        count_.fetch_add(1, std::memory_order_release);
        asio::post(io_ctx_, [this, service_type = std::move(service_type), conf = std::move(conf), creatorid, sessionid](){
            do
            {
                size_t counter = 0;
                uint32_t serviceid = 0;
                do
                {
                    if (counter >= worker::MAX_SERVICE)
                    {
                        serviceid = 0;
                        CONSOLE_ERROR(server_->logger()
                            , "new service failed: can not get more service id. worker[%u] service num[%zu].", id(), services_.size());
                        break;
                    }

                    ++nextid_;
                    if (nextid_ == MAX_SERVICE)
                    {
                        nextid_ = 1;
                    }
                    serviceid = nextid_ | (id() << WORKER_ID_SHIFT);
                    ++counter;
                } while (services_.find(serviceid) != services_.end());

                if (serviceid == 0)
                {
                    break;
                }

                auto s = server_->make_service(service_type);
                MOON_ASSERT(s,
                    moon::format("new service failed:service type[%s] was not registered", service_type.data()).data());
                s->set_id(serviceid);
                s->logger(server_->logger());
                s->set_unique(conf.unique);
                s->set_server_context(server_, this);

                if (!s->init(conf))
                {
                    if (serviceid == BOOTSTRAP_ADDR)
                    {
                        server_->set_state(state::stopping);
                    }
                    break;
                }
                s->ok(true);
                services_.emplace(serviceid, std::move(s));

                if (0 != sessionid)
                {
                    server_->response(creatorid, std::string_view{}, std::to_string(serviceid), sessionid);
                }
                return;
            } while (false);

            count_.fetch_sub(1, std::memory_order_release);
            if (services_.empty())
            {
                shared(true);
            }

            if (0 != sessionid)
            {
                server_->response(creatorid, std::string_view{}, "0"sv, sessionid);
            }
        });
    }

    void worker::remove_service(uint32_t serviceid, uint32_t sender, uint32_t sessionid)
    {
        asio::post(io_ctx_, [this, serviceid, sender, sessionid]() {
            if (auto s = find_service(serviceid); nullptr != s)
            {
                count_.fetch_sub(1, std::memory_order_release);

                auto content = moon::format(R"({"name":"%s","serviceid":%08X,"errmsg":"service destroy"})", s->name().data(), s->id());
                server_->response(sender, "service destroy"sv, content, sessionid);
                services_.erase(serviceid);
                if (services_.empty()) shared(true);

                if (server_->get_state() == state::ready)
                {
                    std::string_view header{ "_service_exit" };
                    auto buf = message::create_buffer();
                    buf->write_back(content.data(), content.size());
                    server_->broadcast(serviceid, buf, header, PTYPE_SYSTEM);
                }

                if (serviceid == BOOTSTRAP_ADDR)
                {
                    server_->set_state(state::stopping);
                }
            }
            else
            {
                server_->response(sender, "worker::remove_service "sv, moon::format("service [%08X] not found", serviceid), sessionid, PTYPE_ERROR);
            }
         });
    }

    asio::io_context& worker::io_context()
    {
        return io_ctx_;
    }

    void worker::send(message_ptr_t&& msg)
    {
        ++mqsize_;
        if (mq_.push_back(std::move(msg)) == 1)
        {
            asio::post(io_ctx_, [this]() {
                if (mq_.size() == 0)
                {
                    return;
                }

                service* ser = nullptr;
                mq_.swap(swapmq_);
                for (auto& msg : swapmq_)
                {
                    handle_one(ser, std::move(msg));
                    --mqsize_;
                }
                swapmq_.clear();
                if (!prefabs_.empty())
                {
                    prefabs_.clear();
                }
            });
        }
    }

    uint32_t worker::id() const
    {
        return workerid_;
    }

    service* worker::find_service(uint32_t serviceid) const
    {
        auto iter = services_.find(serviceid);
        if (services_.end() != iter)
        {
            return iter->second.get();
        }
        return nullptr;
    }

    intptr_t worker::make_prefab(moon::buffer_ptr_t buf)
    {
        intptr_t  prefabid = (intptr_t)(buf.get());
        auto iter = prefabs_.emplace(prefabid, std::move(buf));
        if (iter.second)
        {
            return iter.first->first;
        }
        return 0;
    }

    bool worker::send_prefab(uint32_t sender, uint32_t receiver, intptr_t prefabid, std::string_view header, int32_t sessionid, uint8_t type) const
    {
        if (auto iter = prefabs_.find(prefabid); iter != prefabs_.end())
        {
            server_->send(sender, receiver, iter->second, header, sessionid, type);
            return true;
        }
        return false;
    }

    void worker::shared(bool v)
    {
        shared_ = v;
    }

    bool worker::shared() const
    {
        return shared_.load();
    }

    void worker::handle_one(service*& s, message_ptr_t&& msg)
    {
        uint32_t sender = msg->sender();
        uint32_t receiver = msg->receiver();
        if (msg->broadcast())
        {
            for (auto& it : services_)
            {
                if (!it.second->unique() && msg->type() == PTYPE_SYSTEM)
                {
                    continue;
                }

                if (it.second->ok() && it.second->id() != sender)
                {
                    handle_message(it.second, std::move(msg));
                }
            }
            return;
        }

        if (nullptr == s || s->id() != receiver)
        {
            s = find_service(receiver);
            if (nullptr == s || !s->ok())
            {
                if (sender != 0 && msg->type()!=PTYPE_TIMER)
                {
                    std::string hexdata = moon::hex_string({ msg->data(),msg->size() });
                    std::string str = moon::format("[%08X] attempt send to dead service [%08X]: %s."
                        , sender
                        , receiver
                        , hexdata.data());

                    msg->set_sessionid(-msg->sessionid());
                    server_->response(sender, "worker::handle_one "sv, str, msg->sessionid(), PTYPE_ERROR);
                }
                return;
            }
        }

        int64_t start_time = moon::time::microsecond();
        handle_message(s, std::move(msg));
        int64_t cost_time = moon::time::microsecond() - start_time;
        s->add_cpu_cost(cost_time);
        cpu_cost_ += cost_time;
        if (cost_time > 100000)
        {
            CONSOLE_WARN(server_->logger(),
                "worker %u handle one message cost %" PRId64 "us, from %08X to %08X", id(), cost_time, sender, receiver);
        }
    }
}

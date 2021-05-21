#include "server.h"
#include "worker.h"
#include "message.hpp"

namespace moon
{
    static uint32_t worker_id(uint32_t serviceid)
    {
        return ((serviceid >> WORKER_ID_SHIFT) & 0xFF);
    }

    server::~server()
    {
        wait();
    }

    void server::init(uint32_t worker_num, const std::string& logfile)
    {
        worker_num = (worker_num == 0) ? 1 : worker_num;

        logger_.init(logfile);

        CONSOLE_INFO(logger(), "INIT with %d workers.", worker_num);

        for (uint32_t i = 0; i != worker_num; i++)
        {
            workers_.emplace_back(std::make_unique<worker>(this,  i + 1));
        }

        for (auto& w : workers_)
        {
            w->run();
        }

        state_.store(state::init, std::memory_order_release);
    }

    void server::run()
    {
        asio::io_context io_context;
        asio::steady_timer timer(io_context);
        asio::error_code ignore;
        bool stop_once = false;

        state_.store(state::ready, std::memory_order_release);
        while (true)
        {
            now_ = time::now();

            if (stopcode_ < 0 )
            {
                break;
            }

            if (stopcode_ > 0 && !stop_once)
            {
                stop_once = true;
                CONSOLE_WARN(logger(), "Received signal code %d", stopcode_);
                for (auto iter = workers_.rbegin(); iter != workers_.rend(); ++iter)
                {
                    (*iter)->stop();
                }
            }

            if (state_.load(std::memory_order_acquire) == state::stopping)
            {
                size_t alive = workers_.size();
                for (const auto& w : workers_)
                {
                    if (w->count_.load(std::memory_order_acquire) == 0)
                    {
                        --alive;
                    }
                }

                if (0 == alive)
                {
                    break;
                }
            }

            timer_.update(now_);
            timer.expires_after(std::chrono::milliseconds(1));
            timer.wait(ignore);
        }
        wait();
    }

    void server::stop(int stopcode)
    {
        stopcode_ = stopcode;
    }

    log* server::logger() const
    {
        return &logger_;
    }

    void server::wait()
    {
        for (auto iter = workers_.rbegin(); iter != workers_.rend(); ++iter)
        {
            (*iter)->wait();
        }
        CONSOLE_INFO(logger(), "STOP");
        logger_.wait();
        state_.store(state::stopped, std::memory_order_release);
    }

    state server::get_state() const
    {
        return state_.load(std::memory_order_acquire);
    }

    void server::set_state(state st)
    {
        state_.store(st, std::memory_order_release);
    }

    std::time_t server::now(bool sync)
    {
        if (sync)
        {
            now_ = time::now();
        }

        if (now_ == 0)
        {
            return time::now();
        }
        return now_;
    }

    uint32_t server::service_count() const
    {
        uint32_t res = 0;
        for (const auto& w : workers_)
        {
            res += w->count_.load(std::memory_order_acquire);
        }
        return res;
    }

    worker* server::next_worker()
    {
        assert(workers_.size() > 0);
        uint32_t min_count = std::numeric_limits<uint32_t>::max();
        uint32_t min_count_workerid = 0;
        for (const auto& w : workers_)
        {
            auto n = w->count_.load(std::memory_order_acquire);
            if (w->shared() && n < min_count)
            {
                min_count = n;
                min_count_workerid = w->id();
            }
        }

        if (min_count_workerid == 0)
        {
            min_count = std::numeric_limits<uint32_t>::max();
            for (const auto& w : workers_)
            {
                auto n = w->count_.load(std::memory_order_acquire);
                if (n < min_count)
                {
                    min_count = n;
                    min_count_workerid = w->id();
                }
            }
        }
        return workers_[min_count_workerid-1].get();
    }

    worker* server::get_worker(uint32_t workerid, uint32_t serviceid) const
    {
        if (workerid == 0)
        {
            workerid = worker_id(serviceid);
        }

        if ((workerid <= 0 || workerid > static_cast<uint32_t>(workers_.size())))
        {
            return nullptr;
        }

        --workerid;
        return workers_[workerid].get();
    }

    uint32_t server::timeout(int64_t interval, uint32_t serviceid)
    {
        return timer_.add(now_+ interval, serviceid, this);
    }

    void server::on_timer(uint32_t serviceid, uint32_t timerid)
    {
        auto msg = message::create(buffer_ptr_t{nullptr});
        msg->set_type(PTYPE_TIMER);
        msg->set_sender(timerid);
        msg->set_receiver(serviceid);
        send_message(std::move(msg));
    }

    void server::new_service(std::string service_type, service_conf conf, uint32_t creatorid, int32_t sessionid)
    {
        worker* w = get_worker(conf.threadid);
        if (nullptr != w)
        {
            w->shared(false);
        }
        else
        {
            w = next_worker();
        }
        w->new_service(std::move(service_type), std::move(conf), creatorid, sessionid);
    }

    void server::remove_service(uint32_t serviceid, uint32_t sender, int32_t sessionid)
    {
        worker* w = get_worker(0, serviceid);
        if (nullptr != w)
        {
            w->remove_service(serviceid, sender, sessionid);
        }
        else
        {
            auto content = moon::format("worker %u not found.", serviceid);
            response(sender, "router::remove_service "sv, content, sessionid, PTYPE_ERROR);
        }
    }

    void server::scan_services(uint32_t sender, uint32_t workerid, int32_t sessionid)
    {
        auto* w = get_worker(workerid);
        if (nullptr == w)
        {
            return;
        }
        asio::post(w->io_context(), [this, sender, w, sessionid] {
            std::string content;
            content.append("[");
            for (auto& it : w->services_)
            {
                content.append(moon::format(
                    R"({"name":"%s","serviceid":"%X"},)",
                    it.second->name().data(),
                    it.second->id()));
            }
            content.back() = ']';
            response(sender, std::string_view{}, content, sessionid);
         });
    }

    bool server::send_message(message_ptr_t&& m) const
    {
        worker* w = get_worker(0, m->receiver());
        if (nullptr == w)
        {
            CONSOLE_ERROR(logger(), "invalid message receiver serviceid %X", m->receiver());
            return false;
        }
        w->send(std::move(m));
        return true;
    }

    bool server::send(uint32_t sender, uint32_t receiver, buffer_ptr_t data, std::string_view header, int32_t sessionid, uint8_t type) const
    {
        sessionid = -sessionid;
        message_ptr_t m = message::create(std::move(data));
        m->set_sender(sender);
        m->set_receiver(receiver);
        if (header.size() != 0)
        {
            m->set_header(header);
        }
        m->set_type(type);
        m->set_sessionid(sessionid);
        return send_message(std::move(m));
    }

    void server::broadcast(uint32_t sender, const buffer_ptr_t& buf, std::string_view header, uint8_t type) const
    {
        for (auto& w : workers_)
        {
            auto m = message::create(buf);
            m->set_broadcast(true);
            m->set_header(header);
            m->set_sender(sender);
            m->set_type(type);
            w->send(std::move(m));
        }
    }

    bool server::register_service(const std::string& type, register_func f)
    {
        auto ret = regservices_.emplace(type, f);
        MOON_ASSERT(ret.second
            , moon::format("already registed service type[%s].", type.data()).data());
        return ret.second;
    }

    service_ptr_t server::make_service(const std::string& type)
    {
        auto iter = regservices_.find(type);
        if (iter != regservices_.end())
        {
            return iter->second();
        }
        return nullptr;
    }

    std::string server::get_env(const std::string& name) const
    {
        std::string v;
        if (env_.try_get_value(name, v))
        {
            return v;
        }
        return std::string{};
    }

    void server::set_env(std::string name, std::string value)
    {
        env_.set(std::move(name), std::move(value));
    }

    uint32_t server::get_unique_service(const std::string& name) const
    {
        if (name.empty())
        {
            return 0;
        }
        uint32_t id = 0;
        unique_services_.try_get_value(name, id);
        return id;
    }

    bool server::set_unique_service(std::string name, uint32_t v)
    {
        if (name.empty())
        {
            return false;
        }
        return unique_services_.try_set(std::move(name), v);
    }

    void server::response(uint32_t to, std::string_view header, std::string_view content, int32_t sessionid, uint8_t mtype)
    {
        if (to == 0 || sessionid == 0)
        {
            if (get_state() == state::ready && mtype == PTYPE_ERROR && !content.empty())
            {
                CONSOLE_DEBUG(logger()
                    , "server::response %s:%s"
                    , std::string(header).data()
                    , std::string(content).data());
            }
            return;
        }

        auto m = message::create(content.size());
        m->set_receiver(to);
        m->set_header(header);
        m->set_type(mtype);
        m->set_sessionid(sessionid);
        m->write_data(content);
        send_message(std::move(m));
    }

    std::string server::info()
    {
        std::string req;
        req.append("[\n");
        req.append(moon::format(R"({"id":0, "socket":%zu, "timer":%zu, "log":%zu})",
            socket_num(),
            timer_.size(),
            logger_.size()
        ));
        for (auto& w : workers_)
        {
            req.append(",\n");
            auto v = moon::format(R"({"id":%u, "cpu":%lld, "mqsize":%u, "service":%u})",
                w->id(),
                w->cpu_cost_,
                w->mqsize_.load(),
                w->count_.load(std::memory_order_acquire)
            );
            w->cpu_cost_ = 0;
            req.append(v);
        }
        req.append("]");
        return req;
    }

    uint32_t server::nextfd()
    {
        uint32_t fd = 0;
        do
        {
            fd = fd_seq_.fetch_add(1);
        } while (fd==0 || !try_lock_fd(fd));
        return fd;
    }

    bool server::try_lock_fd(uint32_t fd)
    {
        std::unique_lock lck(fd_lock_);
        return fd_watcher_.emplace(fd).second;
    }

    void server::unlock_fd(uint32_t fd)
    {
        std::unique_lock lck(fd_lock_);
        size_t count = fd_watcher_.erase(fd);
        MOON_CHECK(count == 1, "socket fd erase failed!");
    }

    size_t server::socket_num()
    {
        std::unique_lock lck(fd_lock_);
        return fd_watcher_.size();
    }
}

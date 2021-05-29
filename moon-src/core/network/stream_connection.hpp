#pragma once
#include "base_connection.hpp"
#include "streambuf.hpp"

namespace moon
{
    class stream_connection : public base_connection
    {
    public:
        using base_connection_t = base_connection;

        template <typename... Args>
        explicit stream_connection(Args&&... args)
            :base_connection_t(std::forward<Args>(args)...)
        {
        }

        void start(bool accepted) override
        {
            base_connection_t::start(accepted);
            response_ = message::create(8192, 0);
        }

        void read(size_t n, std::string_view delim, int32_t sessionid) override
        {
            if (!is_open() || sessionid_ != 0)
            {
                asio::post(socket_.get_executor(), [this, self = shared_from_this()]() {
                    error(make_error_code(error::invalid_read_operation));
                });
                return;
            }

            buffer* buf = response_->get_buffer();
            std::size_t size = buf->size() + delim_.size();
            buf->commit(revert_);
            buf->consume(size);
            revert_ = 0;

            delim_ = delim;
            sessionid_ = sessionid;

            if (!delim_.empty())
            {
                read_until((n > 0 ? n : std::numeric_limits<size_t>::max()));
            }
            else
            {
                read(n);
            }
        }

    protected:
        void read_until(size_t count)
        {
            asio::async_read_until(socket_, moon::streambuf(response_->get_buffer(), count), delim_,
                    [this, self = shared_from_this()](const asio::error_code& e, std::size_t bytes_transferred)
            {
                if (e)
                {
                    error(e);
                    return;
                }
                recvtime_ = now();
                response(bytes_transferred, response_);
            });
        }

        void read(size_t count)
        {
            std::size_t size = (response_->size() >= count ? 0 : (count - response_->size()));
            asio::async_read(socket_, moon::streambuf(response_->get_buffer(), count), asio::transfer_exactly(size),
                    [this, self = shared_from_this(), count](const asio::error_code& e, std::size_t)
            {
                if (e)
                {
                    error(e);
                    return;
                }
                recvtime_ = now();
                response(count, response_);
            });
        }

        void error(const asio::error_code& e, const std::string& additional = "") override
        {
            (void)additional;

            if (nullptr == parent_)
            {
                return;
            }
            delim_.clear();
            response_->get_buffer()->clear();

            if (e)
            {
                if (e == moon::error::read_timeout)
                {
                    response_->set_header("TIMEOUT");
                }
                else if (e == asio::error::eof)
                {
                    response_->set_header("EOF");
                }
                else
                {
                    response_->set_header("SOCKET_ERROR");
                    response_->write_data(moon::format("%s.(%d)", e.message().data(), e.value()));
                }
            }

            if (sessionid_ != 0)
            {
                response(0, response_, PTYPE_ERROR);
            }
            parent_->close(fd_);
            parent_ = nullptr;
        }

        void response(size_t count, const message_ptr_t& m, uint8_t type = PTYPE_TEXT)
        {
            if (sessionid_ == 0)
            {
                return;
            }
            auto buf = response_->get_buffer();
            assert(buf->size() >= count);
            revert_ = 0;
            if (type == PTYPE_TEXT)
            {
                revert_ = (buf->size() - count) + delim_.size();
            }   
            buf->revert(revert_);
            m->set_type(type);
            m->set_sender(fd());
            m->set_sessionid(sessionid_);
            sessionid_ = 0;
            handle_message(m);
        }
    protected:
        size_t revert_ = 0;
        int32_t sessionid_ = 0;
        std::string delim_;
        message_ptr_t response_;
    };
}
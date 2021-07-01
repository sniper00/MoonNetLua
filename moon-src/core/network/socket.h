#pragma once
#include "config.hpp"
#include "common/rwlock.hpp"
#include "common/utils.hpp"
#include "asio.hpp"
#include "service.hpp"

namespace moon
{
    class worker;
    class service;
    class base_connection;

    using connection_ptr_t = std::shared_ptr<base_connection>;

    class socket
    {
        struct acceptor_context
        {
            acceptor_context(uint8_t t, uint32_t o, asio::io_context& ioc)
                :type(t)
                , owner(o)
                , acceptor(ioc)
            {

            }

            uint8_t type;
            uint32_t owner;
            uint32_t fd = 0;
            asio::ip::tcp::acceptor acceptor;
        };

        using acceptor_context_ptr_t = std::shared_ptr<acceptor_context>;
    public:
        friend class base_connection;

        socket(server* s, worker* w, asio::io_context& ioctx);

        socket(const socket&) = delete;

        socket& operator =(const socket&) = delete;

        bool try_open(const std::string& host, uint16_t port);

        uint32_t listen(const std::string& host, uint16_t port, uint32_t owner, uint8_t type);

        void accept(uint32_t fd, int32_t sessionid, uint32_t owner);

        uint32_t connect(const std::string& host, uint16_t port, uint32_t owner, uint8_t type, int32_t sessionid, uint32_t millseconds = 0);

        void read(uint32_t fd, uint32_t owner, size_t n, std::string_view delim, int32_t sessionid);

        bool write(uint32_t fd, buffer_ptr_t data, buffer_flag flag = buffer_flag::none);

        bool close(uint32_t fd);

        void close_all();

        bool settimeout(uint32_t fd, uint32_t seconds);

        bool setnodelay(uint32_t fd);

        bool set_enable_chunked(uint32_t fd, std::string_view flag);

        bool set_send_queue_limit(uint32_t fd, uint32_t warnsize, uint32_t errorsize);

		std::string getaddress(uint32_t fd);
    private:
        connection_ptr_t make_connection(uint32_t serviceid, uint8_t type);

        void response(uint32_t sender, uint32_t receiver, std::string_view data, std::string_view header, int32_t sessionid, uint8_t type);

        void add_connection(socket* from, const acceptor_context_ptr_t& ctx, const connection_ptr_t & c, int32_t  sessionid);

        template<typename Message>
        void handle_message(uint32_t serviceid, Message&& m);

        service* find_service(uint32_t serviceid);

        void timeout();
    private:
        server* server_;
        worker* worker_;
        asio::io_context& ioc_;
        asio::steady_timer timer_;
        message_ptr_t  response_;
        std::unordered_map<uint32_t, acceptor_context_ptr_t> acceptors_;
        std::unordered_map<uint32_t, connection_ptr_t> connections_;
    };

    template<typename Message>
    void socket::handle_message(uint32_t serviceid, Message&& m)
    {
        auto s = find_service(serviceid);
        if (nullptr == s)
        {
            close(m->sender());
            return;
        }
        moon::handle_message(s, std::forward<Message>(m));
    }
}

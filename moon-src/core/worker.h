#pragma once
#include "config.hpp"
#include "common/concurrent_queue.hpp"
#include "network/socket.h"

namespace moon
{
    class server;

    class worker
    {
        using queue_type = concurrent_queue<message_ptr_t, std::mutex, std::vector>;

        using command_hander_type = std::function<std::string(const std::vector<std::string_view>&)>;

        using asio_work_type = asio::executor_work_guard<asio::io_context::executor_type>;
    public:
        static constexpr uint32_t MAX_SERVICE = 0xFFFFFF;

        friend class server;

        friend class socket;

        explicit worker(server* srv,  uint32_t id);

        ~worker();

        worker(const worker&) = delete;

        worker& operator=(const worker&) = delete;

        void remove_service(uint32_t serviceid, uint32_t sender, uint32_t sessionid);

        asio::io_context& io_context();

        uint32_t id() const;

        void new_service(std::string service_type, service_conf conf, uint32_t creatorid, int32_t sessionid);

        void send(message_ptr_t&& msg);

        void shared(bool v);

        bool shared() const;

        intptr_t make_prefab(moon::buffer_ptr_t buf);

        bool send_prefab(uint32_t sender, uint32_t receiver, intptr_t prefabid, std::string_view header, int32_t sessionid, uint8_t type) const;

        moon::socket& socket() { return *socket_; }
    private:
        void run();

        void stop();

        void wait();
    private:
        void handle_one(service*& ser, message_ptr_t&& msg);

        service* find_service(uint32_t serviceid) const;
    private:
        std::atomic_bool shared_ = true;
        std::atomic_uint32_t count_ = 0;
        std::atomic_uint32_t mqsize_ = 0;
        uint32_t nextid_ = 0;
        int64_t cpu_cost_ = 0;
        uint32_t workerid_;
        server*  server_;
        asio::io_context io_ctx_;
        asio_work_type work_;
        std::thread thread_;
        queue_type mq_;
        queue_type::container_type swapmq_;
        std::unique_ptr<moon::socket> socket_;
        std::unordered_map<uint32_t, service_ptr_t> services_;
        std::unordered_map<intptr_t, moon::buffer_ptr_t> prefabs_;
    };
};



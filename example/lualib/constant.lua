-- enum  class protocol_type:std::uint8_t
-- {
--     protocol_default,
--     protocol_custom,
--     protocol_websocket
-- };

-- enum class socket_data_type :std::uint8_t
-- {
--     socket_connect = 1,
--     socket_accept =2,
--     socket_recv = 3,
--     socket_close =4,
--     socket_error = 5,
--     socket_logic_error = 6
-- };

-- enum class network_logic_error :std::uint8_t
-- {
--     ok = 0,
--     read_message_size_max = 1, // recv message size too long
--     send_message_size_max = 2, // send message size too long
--     timeout = 3, //socket read time out
--     client_close = 4, //closed
-- };

-- const uint8_t PTYPE_UNKNOWN = 0;
-- const uint8_t PTYPE_SYSTEM = 1;
-- const uint8_t PTYPE_TEXT = 2;
-- const uint8_t PTYPE_LUA = 3;
-- const uint8_t PTYPE_SOCKET = 4;
-- const uint8_t PTYPE_ERROR = 5;
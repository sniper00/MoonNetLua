local moon = require("moon")

---@type asio
local core = require("asio")

local tointeger = math.tointeger
local yield = coroutine.yield
local make_response = moon.make_response
local id = moon.addr()

local accept = core.accept
local connect = core.connect
local read = core.read
local write = core.write

local flag_close = 2
local flag_ws_text = 16
local flag_ws_ping = 32
local flag_ws_pong = 64

local supported_protocol = {
    [moon.PTYPE_TEXT] = true,
    [moon.PTYPE_SOCKET] = true,
    [moon.PTYPE_SOCKET_WS] = true
}

---@class socket : asio
local socket = core

--- async
function socket.accept(listenfd, serviceid)
    serviceid = serviceid or id
    local sessionid = make_response()
    accept(listenfd, sessionid, serviceid)
    local fd,err = yield()
    if not fd then
        return nil,err
    end
    return tointeger(fd)
end

function socket.start(listenfd)
    accept(listenfd, 0, id)
end

--- async
--- param protocol moon.PTYPE_TEXT、moon.PTYPE_SOCKET、moon.PTYPE_SOCKET_WS、
--- timeout millseconds
---@param host string
---@param port integer
---@param protocol integer
---@param timeout integer
function socket.connect(host, port, protocol, timeout)
    assert(supported_protocol[protocol],"not support")
    timeout = timeout or 0
    local sessionid = make_response()
    connect(host, port, id, protocol, sessionid, timeout)
    local fd,err = yield()
    if not fd then
        return nil,err
    end
    return tointeger(fd)
end

function socket.sync_connect(host, port, protocol)
    assert(supported_protocol[protocol],"not support")
    local fd = connect(host, port, id, protocol, 0, 0)
    if fd == 0 then
        return nil,"connect failed"
    end
    return fd
end

--- async
function socket.read(fd, len)
    local sessionid = make_response()
    read(fd, id, len, "", sessionid)
    return yield()
end

--- async
function socket.readline(fd, delim, limit)
    limit= limit or 0
    local sessionid = make_response()
    read(fd, id, limit, delim, sessionid)
    return yield()
end

function socket.write_then_close(fd, data)
    write(fd ,data, flag_close)
end

--- only for websocket
function socket.write_text(fd, data)
    write(fd ,data, flag_ws_text)
end

function socket.write_ping(fd, data)
    write(fd ,data, flag_ws_ping)
end

function socket.write_pong(fd, data)
    write(fd ,data, flag_ws_pong)
end

local socket_data_type = {
    connect = 1,
    accept = 2,
    message = 3,
    close = 4,
    error = 5,
    ping = 6,
    pong = 7,
}

--- tow bytes len protocol callbacks
local callbacks = {}

--- websocket protocol wscallbacks
local wscallbacks = {}

local _decode = moon.decode

moon.dispatch(
    "socket",
    function(msg)
        local fd, sdt = _decode(msg, "SR")
        local f = callbacks[sdt]
        if f then
            f(fd, msg)
        end
    end
)

moon.dispatch(
    "websocket",
    function(msg)
        local fd, sdt = _decode(msg, "SR")
        local f = wscallbacks[sdt]
        if f then
            f(fd, msg)
        end
    end
)

---param name socket_data_type's key
---@param name string
function socket.on(name, cb)
    local n = socket_data_type[name]
    if n then
        callbacks[n] = cb
    else
        error("register unsupport socket data type "..name)
    end
end

---param name socket_data_type's key
---@param name string
function socket.wson(name, cb)
    local n = socket_data_type[name]
    if n then
        wscallbacks[n] = cb
    else
        error("register unsupport websocket data type "..name)
    end
end

return socket

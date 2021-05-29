local http = require("http")
local seri = require("seri")
local moon = require("moon")
local socket = require("moon.socket")

local tbinsert = table.insert
local tbconcat = table.concat

local tostring = tostring
local tonumber = tonumber
local pairs = pairs
local assert = assert
local error = error

local parse_response = http.parse_response
local create_query_string = http.create_query_string

-----------------------------------------------------------------

local function read_chunked(fd)

    local chunkdata = {}

    while true do
        local data, err = socket.readline(fd, "\r\n")
        if not data then
            return {socket_error = err}
        end
        local length = tonumber(data,"16")
        if not length then
            error("Invalid response body")
        end

        if length==0 then
            break
        end

        if length >0 then
            data, err = socket.read(fd, length)
            if not data then
                return {socket_error = err}
            end
            tbinsert( chunkdata, data )
            data, err = socket.readline(fd, "\r\n")
            if not data then
                return {socket_error = err}
            end
        elseif length <0 then
            error("Invalid response body")
        end
    end

    local  data, err = socket.readline(fd, "\r\n")
    if not data then
        return {socket_error = err}
    end

    return chunkdata
end

local function response_handler(fd)
    local data, err = socket.readline(fd, "\r\n\r\n")
    if not data then
        return {socket_error = err}
    end

    --print("raw data",data)
    local ok, version, status_code, header = parse_response(data)
    assert(ok,"Invalid HTTP response header")

    local response = {
        version = version,
        status_code = status_code,
        header = header
    }

    response.header = {}
    for k,v in pairs(header) do
        response.header[k:lower()]=v
    end

    header = response.header

    local content_length = header["content-length"]
    if content_length then
        content_length = tonumber(content_length)
        if not content_length then
            moon.warn("content-length is not number")
            return response
        end

        if content_length >0 then
            --print("Content-Length",content_length)
            data, err = socket.read(fd, content_length)
            if not data then
                return {socket_error = err}
            end
            response.content = data
        end
    elseif header["transfer-encoding"] == 'chunked' then
        local chunkdata = read_chunked(fd)
        if chunkdata.socket_error then
            return chunkdata
        end
        response.content = tbconcat( chunkdata )
    else
        moon.warn("Unsupport transfer-encoding:"..tostring(header["transfer-encoding"]))
    end
    return response
end

local function parse_host(host, defaultport)
    local host_, port = host:match("([^:]+):?(%d*)$")
    if port == "" then
        port = defaultport
    else
        port = tonumber(port)
    end
    return host_, port
end

local M = {}

local default_connect_timeout = 1000

local default_read_timeout = 10000

local keep_alive_host = {}

local max_pool_num = 10

---@param options HttpOptions
local function do_request(baseaddress, options, req)
    options.connect_timeout = options.connect_timeout or default_connect_timeout
    options.read_timeout = options.read_timeout or default_read_timeout

::TRY_AGAIN::
    local fd, err
    local pool = keep_alive_host[baseaddress]
    if not pool then
        pool = {}
        keep_alive_host[baseaddress] = pool
    elseif #pool >0 then
        fd = table.remove(pool)
    end

    local newconn = false
    if not fd then
        local host, port = parse_host(baseaddress, 80)
        fd, err = socket.connect(host, port,  moon.PTYPE_TEXT, options.connect_timeout)
        if not fd then
            return false ,err
        end
        local read_timeout = options.read_timeout or 0
        socket.settimeout(fd, read_timeout//1000)
        newconn = true
    end

    if not socket.write(fd, seri.concat(req)) then
        goto TRY_AGAIN
    end

    local ok , response = pcall(response_handler, fd)
    if not ok then
        socket.close(fd)
        return false, response
    end

    if response.socket_error then
        socket.close(fd)
        if response.socket_error == "TIMEOUT" then
            return false, response.socket_error
        end
        if newconn and response.socket_error == "EOF" then
            return false, response.socket_error
        end
        goto TRY_AGAIN
    end

    if not options.keepalive then
        socket.close(fd)
    else
        if response.header["connection"] == "close" then
            socket.close(fd)
        else
            if #pool < max_pool_num then
                table.insert(pool, fd)
            else
                socket.close(fd)
            end
        end
    end
    return ok, response
end

---@param method string
---@param baseaddress string
---@param options HttpOptions
---@param content string
local function request( method, baseaddress, options, content)

    local host, port = parse_host(baseaddress, 80)

    if not options.path or options.path== "" then
        options.path = "/"
    end

    if options.proxy then
        options.path = "http://"..host..':'..port..options.path
    end

    local cache = {}
    tbinsert( cache, method )
    tbinsert( cache, " " )
    tbinsert( cache, options.path )
    tbinsert( cache, " HTTP/1.1\r\n" )
    tbinsert( cache, "Host: " )
    tbinsert( cache, baseaddress )
    tbinsert( cache, "\r\n")

    if options.header then
        for k,v in pairs(options.header) do
            tbinsert( cache, k)
            tbinsert( cache, ": ")
            tbinsert( cache, tostring(v))
            tbinsert( cache, "\r\n")
        end
    end

    if content and #content > 0 then
        options.header = options.header or {}
        local v = options.header["Content-Length"]
        if not v then
            v = options.header["Transfer-Encoding"]
            if not v or v~="chunked" then
                tbinsert( cache, "Content-Length: ")
                tbinsert( cache, tostring(#content))
                tbinsert( cache, "\r\n")
            end
        end
    end

    if options.keepalive then
        tbinsert( cache, "Connection: keep-alive")
        tbinsert( cache, "\r\n")
        tbinsert( cache, "Keep-Alive: "..tostring(options.keepalive))
        tbinsert( cache, "\r\n")
    end
    tbinsert( cache, "\r\n")
    tbinsert( cache, content)


    if options.proxy then
        baseaddress = options.proxy
    end

    local ok, response = do_request(baseaddress, options, cache)

    if ok then
        return response
    else
        return false, response
    end
end

M.create_query_string = create_query_string

---@class HttpOptions
---@field public path string
---@field public header table<string,string>
---@field public keepalive integer @ seconds
---@field public connect_timeout integer @ ms
---@field public read_timeout integer @ ms
---@field public proxy string @ host:port

---@param host string @host:port
---@param options HttpOptions
function M.get(host, options)
    return request("GET", host, options)
end

---@param host string @host:port
---@param options HttpOptions
function M.put(host, content, options)
    return request("PUT", host, options, content)
end

---@param host string @host:port
---@param content string
---@param options HttpOptions
function M.post(host, content, options)
    return request("POST", host, options, content)
end

---@param host string @host:port
---@param form table @
---@param options HttpOptions
function M.postform(host, form, options)
    options.header = options.header or {}
    options.header["content-type"] = "application/x-www-form-urlencoded"
    for k,v in pairs(form) do
        form[k] = tostring(v)
    end
    return request("POST", host, options, create_query_string(form))
end

return M


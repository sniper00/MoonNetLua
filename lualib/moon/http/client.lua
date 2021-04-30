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

local timeout  = 0

local proxyaddress = nil

local keep_alive_host = {}

local max_pool_num = 10

local function do_request(baseaddress, keepalive, req)

::TRY_AGAIN::
    local fd, err
    local pool = keep_alive_host[baseaddress]
    if not pool then
        pool = {}
        keep_alive_host[baseaddress] = pool
    elseif #pool >0 then
        fd = table.remove(pool)
    end

    if not fd then
        local host, port = parse_host(baseaddress, 80)
        fd, err = socket.connect(host, port,  moon.PTYPE_TEXT, timeout)
        if not fd then
            return false ,err
        end
        socket.settimeout(fd, timeout//1000)
    end

    if not socket.write(fd, seri.concat(req)) then
        fd = nil
        goto TRY_AGAIN
    end

    local ok , response = pcall(response_handler, fd)
    if not ok then
        socket.close(fd)
        return false, response
    end

    if response.socket_error then
        socket.close(fd)
        fd = nil
        if tostring(response.socket_error):find("timeout") then
            return false, "read timeout"
        end
        goto TRY_AGAIN
    end

    if not keepalive then
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

local function request( method, baseaddress, path, content, header, keepalive)

    local host, port = parse_host(baseaddress, 80)

    if not path or path== "" then
        path = "/"
    end

    if proxyaddress then
        path = "http://"..host..':'..port..path
    end

    local cache = {}
    tbinsert( cache, method )
    tbinsert( cache, " " )
    tbinsert( cache, path )
    tbinsert( cache, " HTTP/1.1\r\n" )
    tbinsert( cache, "Host: " )
    tbinsert( cache, baseaddress )
    tbinsert( cache, "\r\n")

    if header then
        for k,v in pairs(header) do
            tbinsert( cache, k)
            tbinsert( cache, ": ")
            tbinsert( cache, tostring(v))
            tbinsert( cache, "\r\n")
        end
    end

    if content and #content > 0 then
        header = header or {}
        local v = header["Content-Length"]
        if not v then
            v = header["Transfer-Encoding"]
            if not v or v~="chunked" then
                tbinsert( cache, "Content-Length: ")
                tbinsert( cache, tostring(#content))
                tbinsert( cache, "\r\n")
            end
        end
    end

    if keepalive then
        tbinsert( cache, "Connection: keep-alive")
        tbinsert( cache, "\r\n")
        tbinsert( cache, "Keep-Alive: "..tostring(keepalive))
        tbinsert( cache, "\r\n")
    end
    tbinsert( cache, "\r\n")
    tbinsert( cache, content)


    if proxyaddress then
        baseaddress = proxyaddress
    end

    local ok, response = do_request(baseaddress, keepalive, cache)
    if not ok then
        --reconnect
        ok, response = do_request(baseaddress, keepalive, cache)
    end

    if ok then
        return response
    else
        return false, response
    end
end

function M.settimeout(v)
    assert(type(v)=="number", "httpclient settimeout need integer param")
    timeout = v
end

function M.setproxy(host)
    proxyaddress = host
end

M.create_query_string = create_query_string

function M.get(host, path, content, header, keepalive)
    return request("GET", host, path, content, header, keepalive)
end

function M.put(host, path, content, header, keepalive)
    return request("PUT", host, path, content, header, keepalive)
end

function M.post(host, path, content, header, keepalive)
    return request("POST", host, path, content, header, keepalive)
end

function M.postform(host, path, form, header, keepalive)
    header = header or {}
    header["content-type"] = "application/x-www-form-urlencoded"

    for k,v in pairs(form) do
        form[k] = tostring(v)
    end
    return request("POST", host, path, create_query_string(form), header, keepalive)
end

return M


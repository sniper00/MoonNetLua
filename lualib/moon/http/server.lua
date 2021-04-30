local moon = require("moon")
local http = require("http")
local seri = require("seri")
---@type fs
local fs = require("fs")
local socket = require("moon.socket")

local parse_request = http.parse_request
local parse_query_string = http.parse_query_string

local tbinsert = table.insert
local tbconcat = table.concat

local strfmt = string.format

local tostring = tostring
local tonumber = tonumber
local setmetatable = setmetatable
local pairs = pairs
local assert = assert

local http_status_msg = {

	[100] = "Continue",

	[101] = "Switching Protocols",

	[200] = "OK",

	[201] = "Created",

	[202] = "Accepted",

	[203] = "Non-Authoritative Information",

	[204] = "No Content",

	[205] = "Reset Content",

	[206] = "Partial Content",

	[300] = "Multiple Choices",

	[301] = "Moved Permanently",

	[302] = "Found",

	[303] = "See Other",

	[304] = "Not Modified",

	[305] = "Use Proxy",

	[307] = "Temporary Redirect",

	[400] = "Bad Request",

	[401] = "Unauthorized",

	[402] = "Payment Required",

	[403] = "Forbidden",

	[404] = "Not Found",

	[405] = "Method Not Allowed",

	[406] = "Not Acceptable",

	[407] = "Proxy Authentication Required",

	[408] = "Request Time-out",

	[409] = "Conflict",

	[410] = "Gone",

	[411] = "Length Required",

	[412] = "Precondition Failed",

	[413] = "Request Entity Too Large",

	[414] = "Request-URI Too Large",

	[415] = "Unsupported Media Type",

	[416] = "Requested range not satisfiable",

	[417] = "Expectation Failed",

	[500] = "Internal Server Error",

	[501] = "Not Implemented",

	[502] = "Bad Gateway",

	[503] = "Service Unavailable",

	[504] = "Gateway Time-out",

	[505] = "HTTP Version not supported",

}

local mimes = {
    [".css"] = "text/css",
    [".htm"] = "text/html; charset=UTF-8",
    [".html"] = "text/html; charset=UTF-8",
    [".js"] = "application/x-javascript",
    [".jpeg"] = "image/jpeg",
    [".ico"] = "image/x-icon"
}

local http_response = {}

http_response.__index = http_response

local static_content

function http_response.new()
    local o = {}
    o.header = {}
    return setmetatable(o, http_response)
end

function http_response:write_header(field,value)
    self.header[tostring(field)] = tostring(value)
end

function http_response:write(content)
    self.content = content
    if content then
        self.header['Content-Length'] = #content
    end
end

function http_response:tb()
    local status_code = self.status_code or 200
    local status_msg = http_status_msg[status_code]
    assert(status_msg,"invalid http status code")

    local cache = {}
    tbinsert( cache, "HTTP/1.1 "..tostring(status_code).." " )
    tbinsert( cache, status_msg )
    tbinsert( cache, "\r\n" )

    for k,v in pairs(self.header) do
        tbinsert( cache, k )
        tbinsert( cache, ": " )
        tbinsert( cache, v )
        tbinsert( cache, "\r\n" )
    end
    tbinsert( cache, "\r\n" )
    if self.content then
        tbinsert( cache, self.content )
    end
    self.header = {}
    self.status_code = nil
    self.content = nil
    return cache
end

----------------------------------------------------------

local M = {}

local routers = {}

local function read_chunked(fd)

    local chunkdata = {}

    while true do
        local data, err = socket.readline(fd, "\r\n")
        assert(data,err)
        local length = tonumber(data,"16")
        if not length then
            error("Invalid response body")
        end

        if length==0 then
            break
        end

        if length >0 then
            data, err = socket.read(fd, length)
            assert(data,err)
            tbinsert( chunkdata, data )
            data, err = socket.readline(fd, "\r\n")
            assert(data,err)
        elseif length <0 then
            error("Invalid response body")
        end
    end

    local  data, err = socket.readline(fd, "\r\n")
    if not data then
        return false,err
    end

    return chunkdata
end

local traceback = debug.traceback

local function request_handler(fd, request)
    local response = http_response.new()
    local conn_type = request.header["Connection"]

    if static_content then
        local request_path = request.path
        local static_src = static_content[request_path]
        if static_src then
            response:write_header("Content-Type", static_src.mime)
            response:write(static_src.bin)
            if conn_type == "close" then
                socket.write_then_close(fd, seri.concat(response:tb()))
            else
                socket.write(fd, seri.concat(response:tb()))
            end
            return
        end
    end

    local handler =  routers[request.path]
    if handler then
        local ok,err = xpcall(handler, traceback, request, response)
        if not ok then
            if M.error then
                M.error(err)
            end
            response.status_code = 500
            response:write_header("Content-Type","text/plain")
            response:write("Server Internal Error")
        end
    else
        response.status_code = 404
        response:write_header("Content-Type","text/plain")
        response:write(string.format("Cannot %s %s", request.method, request.path))
    end

    if conn_type == "close" then
        socket.write_then_close(fd, seri.concat(response:tb()))
    else
        socket.write(fd, seri.concat(response:tb()))
    end
end

local function session_handler(fd)
    local data, err = socket.readline(fd, "\r\n\r\n", M.header_max_len)
    assert(data,err)

    --print("raw data",data)
    local ok,method,path,query_string,version,header = parse_request(data)
    assert(ok,"Invalid HTTP response header")

    local request = {
        method = method,
        path = path,
        query_string = query_string,
        version = version
    }

    request.header = {}
    for k,v in pairs(header) do
        request.header[k:lower()]=v
    end

    header = request.header

    request.parse_query = function() return parse_query_string(request.query_string) end

    request.parse_form = function() return parse_query_string(request.content) end

    local content_length = header["content-length"]
    if content_length then
        content_length = tonumber(content_length)
        if not content_length then
            error("content-length is not number")
        end

        if M.content_max_len and content_length> M.content_max_len then
            socket.close(fd)
            error(strfmt( "content length %d, limit %d",content_length,M.content_max_len ))
        end

        data, err = socket.read(fd, content_length)
        assert(data,err)
        --print("Content-Length",content_length)
        request.content = data
    elseif header["transfer-encoding"] == 'chunked' then
        local chunkdata = read_chunked(fd)
        if not chunkdata then
            error("Invalid response body")
        end
        request.content = tbconcat( chunkdata )
    elseif method:upper()~="GET" then
        moon.warn("Unsupport transfer-encoding: "..tostring(header["transfer-encoding"]))
    end
    request_handler(fd, request)
end

-----------------------------------------------------------------

local listenfd

function M.start(fd, timeout)
    socket.settimeout(fd, timeout)
    moon.async(function()
        while true do
            local ok,errmsg = pcall(session_handler,fd)
            if not ok then
                if M.error then
                    M.error(fd, errmsg)
                else
                    print("httpserver session error",errmsg)
                end
                return
            end
        end
    end)--async
end

function M.listen(host,port,timeout)
    assert(not listenfd,"http server can only listen port once.")
    listenfd = socket.listen(host, port, moon.PTYPE_TEXT)
    timeout = timeout or 0
    moon.async(function()
        while true do
            local fd,err = socket.accept(listenfd, moon.id)
            if not fd then
                print("httpserver accept",err)
                return
            end
            M.start(fd, timeout)
        end--while
    end)
end

function M.on( path, cb )
    routers[path] = cb
end

function M.static(dir, showdebug)
    static_content = {}
    dir = fs.abspath(dir)
    local res = fs.listdir(dir, 100)
    for _, v in ipairs(res) do
        local src = string.gsub(v, dir, "")
        src = string.gsub(src, "\\","/")
        if string.sub(src,1,1) ~= "/" then
            src = "/"..src
        end

        if not fs.isdir(v) then
            local ext = fs.ext(src)
            local mime = mimes[ext] or ext
            static_content[src] = {
                mime = mime,
                bin = io.readfile(v)
            }
            if showdebug then
                print("load static file:", src)
            end
        else
            local index_html = fs.join(v,"index.html")
            if fs.exists(index_html) then
                static_content[src] = {
                    mime = mimes[".html"],
                    bin = io.readfile(index_html)
                }
                static_content[src.."/"] = static_content[src]
                if showdebug then
                    print("load static index:", src)
                    print("load static index:", src.."/")
                end
            end
        end
    end

    if fs.exists(fs.join(dir,"index.html")) then
        static_content["/"] = {
            mime = mimes[".html"],
            bin = io.readfile(fs.join(dir,"index.html"))
        }
    end
end

return M

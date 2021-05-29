local moon = require("moon")
local httpc = require("moon.http.client")

local http_server = require("moon.http.server")

-- http_server.header_max_len = 8192
-- http_server.content_max_len = 8192

http_server.error = function(fd, err)
    print("http server fd",fd," disconnected:",  err)
end

http_server.on("/get",function(request, response)
    print_r(request.parse_query(request.query_string),"GET")
    response:write_header("Content-Type","text/plain")
    response:write("GET:Hello World")
end)

http_server.on("/post",function(request, response)
    print_r(request.content,"POST")
    response:write_header("Content-Type","text/plain")
    response:write("POST:Hello World/home")
end)

http_server.on("/postform",function(request, response)
    print_r(request.parse_form(request.content),"POST_FORM")
    response:write_header("Content-Type","text/plain")
    response:write("POST_FROM:Hello World/home")
end)

http_server.listen("127.0.0.1",8001)
print("http_server start", "127.0.0.1",8001)


-- moon.async(function ()
--     print_r(httpc.get("www.google.com:443"),{
--         proxy = "127.0.0.1:8443"
--     })
-- end)

moon.async(function ()
    httpc.get("127.0.0.1:8001", {
        path = "/get?a=1&b=2",
        keepalive = 300
    })

    httpc.post("127.0.0.1:8001","Hello Post", {
        path = "/post",
        keepalive = 300
    })

    local form = {username="wang",passwd="456",age=110}
    httpc.postform("127.0.0.1:8001", form, {
        path = "/postform",
        keepalive = 300
    })

    moon.exit(100)
end)

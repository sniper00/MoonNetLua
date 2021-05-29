local moon = require("moon")
local http_server = require("moon.http.server")
local httpc = require("moon.http.client")
local test_assert = require("test_assert")
-- http_server.header_max_len = 8192
-- http_server.content_max_len = 8192

http_server.error = function(fd, err)
    print("http server fd",fd," disconnected:",  err)
end

http_server.on("/home",function(request, response)
    -- print_r(request)
    -- print_r(request.parse_query_string(request.query_string))
    test_assert.equal(request.content,"HAHAHA")
    response:write_header("Content-Type","text/plain")
    response:write("Hello World/home")
end)

http_server.listen("127.0.0.1",8001)

moon.async(function()
    local response = httpc.post("127.0.0.1:8001","HAHAHA", {
        path = "/home"
    })
    -- print_r(response)
    test_assert.equal(response.content,"Hello World/home")
    test_assert.success()
end)

local moon = require("moon")
local seri = require("seri")
local sharetable = require("sharetable")

local conf = ... or {}

local name = "sharedata"

if conf.agent then
    local data
    local command = {}

    command.LOAD = function()
        local res = sharetable.queryall()
        for k, v in pairs(res) do
            print(k, v)
        end
        data = res[name]
        print_r(data)
        return true
    end

    command.UPDATE = function()
        data = sharetable.query(name..".lua")
        print_r(data)
    end

    moon.dispatch('lua',function(msg)
        local sender, sessionid, buf = moon.decode(msg, "SEB")
        local cmd, sz, len = seri.unpack_one(buf)
        local f = command[cmd]
        if f then
            moon.async(function()
                moon.response("lua", sender, sessionid, f(seri.unpack(sz, len)))
            end)
        else
            moon.error(moon.name, "recv unknown cmd "..cmd)
        end
    end)
else
    local content_old = [[
        local M = {
            a = 1,
            b = "hello",
            c = {
                1,2,3,4,5
            }
        }
        return M
    ]]

    local content_new = [[
        local M = {
            a = 2,
            b = "world",
            c = {
                7,8,9,10,11
            }
        }
        return M
    ]]

    local agent = 0
    moon.async(function()

        io.writefile("./table/"..name..".lua", content_old)

        moon.new_service("lua",{
            unique = true,
            name = "sharetable",
            file = "../service/sharetable.lua",
            dir = "./table"
        })

        agent =  moon.new_service(
            "lua",
            {
                name = "agent",
                file = "start_by_config/sharetable_example_dir.lua",
                agent = true
            }
        )

        print(moon.co_call("lua", agent, "LOAD"))

        io.writefile("./table/"..name..".lua", content_new)
        print(sharetable.loadfile(name..".lua"))
        print(moon.co_call("lua", agent, "UPDATE"))

        moon.remove_service(agent)
    end)
end


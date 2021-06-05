local moon   = require("moon")

local command = {}

command.ADD =  function(a,b)
    return a+b
end

command.SUB = function(a,b)
    return a-b
end

command.MUL = function(a,b)
    return a*b
end

command.ACCUM = function(...)
    local numbers = {...}
    local total = 0
    for _,v in pairs(numbers) do
        total = total + v
    end
    return total
end

local count = 0
local tcount = 0
local bt = 0
command.COUNTER = function(t)
    -- print(...)
    count = count + 1
    if bt == 0 then
        bt = t
    end
    tcount = tcount + 1

    if count == 10000 then
        print("per", (t-bt))
        count = 0
        bt = 0
    end
end

moon.async(function()
    while true do
        moon.sleep(1000)
        print(tcount)
    end
end)


local function docmd(sender,sessionid, CMD,...)
    local f = command[CMD]
    if f then
        if CMD ~= 'ADD' then
            local args = {...}
            moon.async(function ()
                --moon.sleep(20000)
                moon.response('lua',sender,sessionid,f(table.unpack(args)))
            end)
        end
    else
        error(string.format("Unknown command %s", tostring(CMD)))
    end
end

moon.dispatch('lua',function(msg,unpack)
    local sender, sessionid, sz, len = moon.decode(msg, "SEC")
    docmd(sender, sessionid, unpack(sz, len))
end)

moon.async(function()
    moon.sleep(10)
    print(moon.co_call("lua", moon.queryservice("cluster"), "Start"))
end)


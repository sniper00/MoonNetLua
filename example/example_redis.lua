local moon = require("moon")
local redis = require("moon.db.redis")

local HOST = "127.0.0.1"
local PORT = 6379

local function run_example()

    local n = 0
    moon.async(function()
        while true do
            moon.sleep(1000)
            print("per sec ",n)
            n = 0
        end
    end)

    moon.async(function()
        local db,err = redis.connect({host = HOST,port = PORT})
        if not db then
            print("error",err)
            return
        end

        db:del "C"
        db:set("A", "hello")
        db:set("B", "world")
        db:sadd("C", "one")

        print(db:get("A"))
        print(db:get("B"))

        db:del "D"
        for i=1,10 do
            db:hset("D",i,i)
        end
        local r = db:hvals "D"
        for k,v in pairs(r) do
            print(k,v)
        end

        db:multi()
        db:get "A"
        db:get "B"
        local t = db:exec()
        for k,v in ipairs(t) do
            print("Exec", v)
        end

        print(db:exists "A")
        print(db:get "A")
        print(db:set("A","hello world"))
        print(db:get("A"))
        print(db:sismember("C","one"))
        print(db:sismember("C","two"))

        print("===========publish============")

        for i=1,10 do
            db:publish("foo", i)
        end
        for i=11,20 do
            db:publish("hello.foo", i)
        end

        db:disconnect()
    end)

    moon.async(function()

        local db,err = redis.connect({host = HOST,port = PORT})
        if not db then
            print(err)
            return
        end

        db:hmset("myhash","name","Ben","age",11)

        repeat
            moon.sleep(1000)

            local res, err = db:pipeline({
                {"set", "cat", "Marry"},
                {"set", "horse", "Bob"},
                {"get", "cat"},
                {"get", "horse"},
                },{}) -- offer a {} for result

            if err then
                print(err)
            else
                print_r(res)
            end

        until(true)
        db:disconnect()
    end)

    moon.async(function()
        local db,err = redis.connect({host = HOST,port = PORT})
        if not db then
            print(err)
            return
        end

        local index = 1
        while true do
            local ret,errmsg = db:set("dog"..tostring(index), "an animaldadadaddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd")
            if not ret then
                print("set", errmsg)
                break
            end
            assert ('OK'== ret,ret)
            n=n+1
            index = index+1
        end
        db:disconnect()
    end)

end

run_example()
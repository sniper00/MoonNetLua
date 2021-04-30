local moon = require("moon")
local seri = require("seri")
local socket = require("moon.socket")
local redis = require("moon.db.redis")

local tbinsert = table.insert
local tbunpack = table.unpack

local conf = ...

if conf.name then
    local function connect(db_conf, auto_reconnect)
        local db, err
        repeat
            db, err = redis.connect(db_conf)
            if not db then
                moon.error(err)
                break
            end

            if db == redis.socket_error then
                db = nil
            end

            if not db and auto_reconnect then
                moon.sleep(1000)
            end
        until(not auto_reconnect or db)
        return db, err
    end

    local function exec_one(db, msg , sender, sessionid, auto_reconnect)
        local reconnect_times = 1

        if auto_reconnect then
            reconnect_times = -1
        end

        repeat
            local err,res
            if not db then
                db, err = connect(conf, auto_reconnect)
                if not db then
                    if sessionid == 0 then
                        moon.error(err)
                    else
                        moon.response("lua", sender, sessionid, false, err)
                    end
                    return
                end
            end
            res, err = redis.raw_send(db, msg)
            if redis.socket_error == res then
                db = nil
                moon.error(err)
                if reconnect_times == 0 then
                    if sessionid ~= 0 then
                        moon.response("lua", sender, sessionid, false, err)
                    end
                    return
                end
            else
                if sessionid == 0 and not res then
                    moon.error(err)
                else
                    moon.response("lua", sender, sessionid, res, err)
                end
                return db, res
            end
        until false
    end

    local db_pool_size = conf.poolsize or 1

    local make_queue = make_queue
    local queue_push = queue_push
    local queue_pop = queue_pop
    local queue_front = queue_front
    local queue_size = queue_size
    local traceback = debug.traceback
    local xpcall = xpcall

    local clone = moon.clone
    local release = moon.release

    local free_all = function(one)
        for k, req in pairs(one.queue) do
            if type(req[1])=="userdata" then
                release(req[1])
            end
        end
        one.queue = {}
    end

    local pool = {}

    for _=1,db_pool_size do
        local one = setmetatable({queue = make_queue(), running = false, db = false},{
            __gc = free_all
        }) 
        tbinsert(pool,one)
    end

    local function docmd(hash, sender, sessionid, msg)
        hash = hash%db_pool_size + 1
        --print(moon.name, "db hash", hash)
        local ctx = pool[hash]
        queue_push(ctx.queue, {clone(msg), sender, sessionid})
        if ctx.running then
            return
        end
        ctx.running = true
        moon.async(function()
            while true do
                ---args - sender - sessionid
                local req = queue_front(ctx.queue)
                if not req then
                    break
                end
                local iscall = req[3]~=0
                local ok,db = xpcall(exec_one, traceback, ctx.db, req[1], req[2], req[3], not iscall)
                if not ok then
                    if ctx.db then
                        ctx.db:disconnect()
                        ctx.db = nil
                    end
                    ---print lua error
                    moon.error(db)
                else
                    ctx.db = db
                end
                if iscall or db then
                    release(req[1])
                    queue_pop(ctx.queue)
                end
            end
            ctx.running = false
        end)
    end

    local fd = socket.sync_connect(conf.host,conf.port,moon.PTYPE_TEXT)
    assert(fd, "connect db redis failed")
    socket.close(fd)

    local buffer = require("buffer")
    local unpack_one = seri.unpack_one
    local buf_write_front = buffer.write_front

    local command = {}

    function command.len()
        local res = {}
        for _,v in ipairs(pool) do
            res[#res+1] = queue_size(v.queue)
        end
        return res
    end

    moon.dispatch('lua',function(msg,unpack)
        local sender, sessionid, buf = moon.decode(msg, "SEB")
        local cmd, sz, len = seri.unpack_one(buf, true)
        if cmd == "Q" then
            local hash = seri.unpack_one(buf, true)
            docmd(hash, sender, sessionid, msg)
            return
        end

        local fn = command[cmd]
        if fn then
            if sessionid == 0 then
                fn(sender, sessionid, buf, msg)
            else
                moon.async(function()
                    if sessionid ~= 0 then
                        local unsafe_buf = seri.pack(xpcall(fn, debug.traceback, unpack(sz, len)))
                        local ok = unpack_one(unsafe_buf, true)
                        if not ok then
                            buf_write_front(unsafe_buf, seri.packs(false))
                        end
                        moon.raw_send("lua", sender, "", unsafe_buf, sessionid)
                    end
                end)
            end
        else
            moon.error(moon.name, "recv unknown cmd "..tostring(cmd))
        end
    end)

    local function wait_all_send()
        while true do
            local all = true
            for _,v in ipairs(pool) do
                if queue_front(v.queue) then
                    all = false
                    print("wait_all_send", _, queue_size(v.queue))
                    break
                end
            end

            if not all then
                moon.sleep(1000)
            else
                return
            end
        end
    end

    moon.system("wait_save", function()
        moon.async(function()
            wait_all_send()
            moon.quit()
        end)
    end)
else
    local json = require("json")
    local buffer = require("buffer")
    local concat_resp = json.concat_resp
    ---@class redis_client
    local client = {}

    local yield = coroutine.yield
    local raw_send = moon.raw_send
    local pack = seri.pack
    local packstr = seri.packs

    local wfront = buffer.write_front

    function client.call(db, ...)
        local sessionid = moon.make_response(db)
        local buf = concat_resp(...)
        assert(wfront(buf, packstr("Q", 1)))
        raw_send("lua", db, "", buf, sessionid)
        return yield()
    end

    function client.hash_call(hash, db, ...)
        local sessionid = moon.make_response(db)
        hash = hash or 1
        local buf = concat_resp(...)
        assert(wfront(buf, packstr("Q", hash)))
        raw_send("lua", db,"", buf, sessionid)
        return yield()
    end

    function client.send(db, ...)
        local buf = concat_resp(...)
        assert(wfront(buf, packstr("Q", 1)))
        raw_send("lua", db, "", buf, 0)
    end

    function client.hash_send(hash, db, ...)
        hash = hash or 1
        local buf = concat_resp(...)
        assert(wfront(buf, packstr("Q", hash)))
        raw_send("lua", db, "", buf, 0)
    end
    return client
end


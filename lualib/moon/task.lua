local moon = require("moon")
local corunning = coroutine.running
local costatus = coroutine.status
local coresume = coroutine.resume
local coyield = coroutine.yield

local traceback = debug.traceback

local ipairs = ipairs

local xpcall = xpcall

local _M = {}

---
--- 等待多个异步调用，并获取结果
function _M.wait_all(fnlist)
    local n = #fnlist
    local res = {}
    if n == 0 then
        return res
    end
    local co = corunning()
    for i,fn in ipairs(fnlist) do
        moon.async(function ()
            res[i] = {xpcall(fn, traceback)}
            if res[i][1] then
                table.remove(res[i], 1)
            end
            n=n-1
            if n==0 then
                if costatus(co) == "suspended" then
                    moon.wakeup(co)
                end
            end
        end)
    end
    coyield()
    return res
end

return _M
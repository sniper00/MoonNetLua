local moon = require("moon")
local task = require("moon.task")
local httpc = require("moon.http.client")

local function fn1()
	return 1,"a","b"
end

local function fn2()
	moon.sleep(5000)
	return 2
end

local function fn3()
	moon.sleep(3000)
	return 3
end

local function fn4()
	moon.sleep(2000)
	return 4
end

moon.async(function()
	local res = task.wait_all({fn1,fn2,fn3,fn4})
	print_r(res)
end)


moon.async(function()
	local res =  task.wait_all({
		function ()
			return httpc.get("www.baidu.com")
		end,
		function ()
			return httpc.get("cn.bing.com")
		end,
		function ()
			moon.sleep(10000)
			assert(false)
		end})
	print_r(res)
	moon.exit(100)
end)


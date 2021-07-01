local moon = require("moon")
local queue = require("moon.queue")

local lock = queue()
local tb = {}

for i=1,1000 do
	local co = coroutine.create(function(v)
		print("lock", v)
		local scope_lock<close> = lock(v)
		if 1==v then
			print(coroutine.yield(v), v)
		end
		print("unlock", v)
	end)
	tb[#tb+1] = co
	coroutine.resume(co,i )
end

print("########")
coroutine.resume( tb[1],"start")

local mutex = queue()

moon.async(function()
	local lock<close> = mutex()
	moon.sleep(1000)
	print(1)
end)

moon.async(function()
	local lock<close> = mutex()
	moon.sleep(2000)
	assert(false)
	print(2)
end)

moon.async(function()
	local lock<close> = mutex()
	moon.sleep(1000)
	print(3)
end)

moon.async(function()
	local lock<close> = mutex()
	moon.sleep(500)
	print(4)

    moon.exit(100)
end)

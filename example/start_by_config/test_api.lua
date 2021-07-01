local moon = require("moon")

local json = require("json")
local seri = require("seri")
local fs = require("fs")
local test_assert = require("test_assert")

local equal = test_assert.equal

equal(type(checkbool) , "function")
equal(type(checkint) , "function")
equal(type(checknumber) , "function")
equal(type(checktable) , "function")

equal(type(class) , "function")
equal(type(iskindof) , "function")

equal(type(moon.name) , "string")
equal(type(moon.id) , "number")
equal(type(moon.send_prefab) , "function")
equal(type(moon.make_prefab) , "function")
equal(type(moon.queryservice) , "function")
equal(type(moon.new_service) , "function")
equal(type(moon.send) , "function")
equal(type(moon.exit) , "function")

equal(type(moon.decode) , "function")
equal(type(moon.clone) , "function")
equal(type(moon.release) , "function")
equal(type(moon.redirect) , "function")

equal(type(fs.mkdir) , "function")
equal(type(fs.cwd) , "function")
print(fs.cwd())
equal(type(fs.exists) , "function")
equal(type(fs.listdir) , "function")

local socket = require("asio")
equal(type(socket.listen) , "function")
equal(type(socket.accept) , "function")
equal(type(socket.connect) , "function")
equal(type(socket.read) , "function")
equal(type(socket.write) , "function")
equal(type(socket.write_message) , "function")
equal(type(socket.close) , "function")
equal(type(socket.settimeout) , "function")
equal(type(socket.setnodelay) , "function")
equal(type(socket.set_enable_chunked) , "function")

equal(type(moon.clock) , "function")

equal(type(moon.timeout) , "function")



print("now server time")
moon.adjtime(1000*1000)--forward 1000s
print("after offset, now server time")

print("*********************api test ok**********************")

moon.make_prefab("123")
moon.make_prefab(seri.pack("1",2,3,{a=1,b=2},nil))

moon.set_env("1","2")
equal(moon.get_env("1"),"2")
moon.set_env("1","3")
equal(moon.get_env("1"),"3")

local Base = class("Base")
function Base:ctor()
end

local Child = class("Child", Base)
function Child:ctor()
	Child.super.ctor(self)
end

local ChildB = class("ChildB", Base)
function ChildB:ctor()
	ChildB.super.ctor(self)
end

local c = Child.new()
local cb = ChildB.new()

equal(iskindof(c, "Child"),true)
equal(iskindof(c, "Base"),true)
equal(iskindof(c, "ChildB"),false)
equal(iskindof(cb, "ChildB"),true)

moon.set_env("env_example","haha")
equal(moon.get_env("env_example"),"haha")

do
	local mt={}
	mt.names = "hahha"
	mt.__index = mt
	local tttt={
		a=1,
		b=2,
	}
	setmetatable(tttt,mt)
	local res = json.encode(tttt)
	test_assert.assert(res=='{"a":1,"b":2}' or res == '{"b":2,"a":1}')
end

do
	local buffer = require("buffer")
	local buf = buffer.unsafe_new(256)
	buffer.write_back(buf, "12345")
	assert(buffer.read(buf,5) == "12345")
	buffer.write_back(buf, "abcde")
	assert(buffer.read(buf,5) == "abcde")
	assert(buffer.size(buf) == 0)

	for i =1,1000 do
		buffer.write_back(buf,"abcde")
	end

	buffer.write_front(buf, "1000")

	assert(buffer.read(buf,4) == "1000")

	buffer.seek(buf,1)
	assert(buffer.read(buf,1) == "b")

	buffer.delete(buf)
end

do
	local zset = require("zset")
	local rank = zset.new(3)
	rank:update(4,1,4)
	rank:update(3,1,3)
	rank:update(2,1,2)
	rank:update(1,1,1)
	assert(rank:rank(1)==1)
	assert(rank:rank(2)==2)
	assert(rank:rank(3)==3)
	assert(rank:rank(4)==0)
	assert(rank:size() == 3)
	local res = rank:range(1,4)
	assert(#res==3)
	rank:clear()
	assert(not rank:range(1,4))
end

do
	local q = make_queue()
	queue_push(q, 4)
	queue_push(q, 3)
	queue_push(q, 2)
	queue_push(q, 1)
	assert(queue_pop(q) == 4)
	assert(queue_pop(q) == 3)
	assert(queue_pop(q) == 2)
	assert(queue_pop(q) == 1)
end

moon.async(function()
	local ncount = 0
	moon.timeout(
		10,
		function()
			ncount = ncount + 1
			equal(ncount,1)
		end
	)

	local co1 = moon.async(function()
		moon.sleep(100)
	end)

	moon.sleep(200)

	--coroutine reuse
	local co2 = moon.async(function()
		moon.sleep(100)
	end)

	-- create new coroutine
	local co3 = moon.async(function()
		moon.sleep(100)
	end)
	local running,free = moon.coroutine_num()
	print(running,free)
	test_assert.equal(free, 0)
	test_assert.equal(running, 3)

	moon.sleep(200)

	test_assert.equal(co1, co2)
	test_assert.assert(co1~=co3 and co2~=co3)
	running,free = moon.coroutine_num()
	test_assert.equal(free, 2)
	test_assert.equal(running, 1)


	for _= 1, 1000 do
		moon.async(function()
			moon.sleep(10)
		end)
	end

	moon.sleep(100)

	--coroutine reuse
	for _= 1, 1000 do
		moon.async(function()
			moon.sleep(1000)
		end)
	end

	moon.sleep(2000)

	running,free = moon.coroutine_num()
	test_assert.equal(free, 1000)

	test_assert.success()
end)


-- This file is modified version from https://github.com/cloudwu/skynet/blob/master/lualib/skynet/db/redis.lua

local moon = require "moon"
local seri = require("seri")
local socket = require "moon.socket"

local tostring = tostring
local tonumber = tonumber
local table = table
local string = string
local assert = assert
local setmetatable = setmetatable
local ipairs = ipairs
local type = type
local select = select
local pairs = pairs

local readline = socket.readline
local read = socket.read

local redis = {}
local command = {}
local meta = {
	__index = command,
	-- DO NOT close channel in __gc
}

local socket_error = setmetatable({}, {__tostring = function() return "[Error: socket]" end })	-- alias for error object

redis.socket_error = socket_error

---------- redis response
local redcmd = {}

redcmd[36] = function(fd, data) -- '$'
	local bytes = tonumber(data)
	if bytes < 0 then
		return true,nil
	end
	local firstline, err = read(fd, bytes+2)
	if firstline then
		return true, string.sub(firstline,1,-3)
	else
		return socket_error, err
	end
end

redcmd[43] = function(_, data) -- '+'
	return true, data
end

redcmd[45] = function(_, data) -- '-'
	return false, data
end

redcmd[58] = function(_, data) -- ':'
	-- todo: return string later
	return true, tonumber(data)
end

local function read_response(fd)
	local result, err = readline(fd, "\r\n")
	if not result then
		return socket_error, err
	end
	local firstchar = string.byte(result)
	local data = string.sub(result,2)
	return redcmd[firstchar](fd,data)
end

redcmd[42] = function(fd, data)	-- '*'
	local n = tonumber(data)
	if n < 0 then
		return true, nil
	end
	local bulk = {}
	local noerr = true
	for i = 1,n do
		local ok, v = read_response(fd)
		if not ok then
			noerr = false
		end
		bulk[i] = v
	end
	return noerr, bulk
end

-------------------

function command:disconnect()
	socket.close(self[1])
	setmetatable(self, nil)
end

-- msg could be any type of value

local function make_cache(f)
	return setmetatable({}, {
		__mode = "kv",
		__index = f,
	})
end

local header_cache = make_cache(function(t,k)
		local s = "\r\n$" .. k .. "\r\n"
		t[k] = s
		return s
	end)

local command_cache = make_cache(function(t,cmd)
		local s = "\r\n$"..#cmd.."\r\n"..cmd:upper()
		t[cmd] = s
		return s
	end)

local count_cache = make_cache(function(t,k)
		local s = "*" .. k
		t[k] = s
		return s
	end)

local command_np_cache = make_cache(function(t, cmd)
		local s = "*1" .. command_cache[cmd] .. "\r\n"
		t[cmd] = s
		return s
	end)

local function compose_message(cmd, msg)
	if msg == nil then
		return command_np_cache[cmd]
	end

	local t = type(msg)
	local lines = {}

	if t == "table" then
		local n = msg.n or #msg
		lines[1] = count_cache[n+1]
		lines[2] = command_cache[cmd]
		local idx = 3
		for i = 1, n do
			local v = msg[i]
			if v == nil then
				lines[idx] = "\r\n$-1"
				idx = idx + 1
			else
				v= tostring(v)
				lines[idx] = header_cache[#v]
				lines[idx+1] = v
				idx = idx + 2
			end
		end
		lines[idx] = "\r\n"
	else
		msg = tostring(msg)
		lines[1] = "*2"
		lines[2] = command_cache[cmd]
		lines[3] = header_cache[#msg]
		lines[4] = msg
		lines[5] = "\r\n"
	end

	return lines
end

local function request(fd, req, res, israw)
	if israw then
		socket.write_message(fd,  req)
	else
		socket.write(fd,  seri.concat(req))
	end
	if not res then
		return true
	end
	local ok, data = res(fd)
	if ok and ok~=socket_error then
		return data
	end
	return ok, data
end

local function redis_login(auth, db)
	if auth == nil and db == nil then
		return
	end
	return function(fd)
		if auth then
			request(fd, compose_message("AUTH", auth), read_response)
		end
		if db then
			request(fd, compose_message("SELECT", db), read_response)
		end
	end
end

function redis.connect(db_conf)
	local fd, err = socket.connect(db_conf.host, db_conf.port or 6379, moon.PTYPE_TEXT, db_conf.timeout)
	if not fd then
		return false, err
	end
	socket.setnodelay(fd)
	local f = redis_login(db_conf.auth, db_conf.db)
	if f then
		f(fd)
	end
	return setmetatable( { fd }, meta )
end

function redis.raw_send(self, data)
	return request(self[1], data, read_response, true)
end

setmetatable(command, { __index = function(t,k)
	local cmd = string.upper(k)
	local f = function (self, v, ...)
		if nil == v then
			return request(self[1], compose_message(cmd), read_response)
		elseif type(v) == "table" then
			return request(self[1], compose_message(cmd, v), read_response)
		else
			return request(self[1], compose_message(cmd, table.pack(v, ...)), read_response)
		end
	end
	t[k] = f
	return f
end})

local function read_boolean(so)
	local ok, result = read_response(so)
	return ok, result ~= 0
end

function command:exists(key)
	return request(self[1], compose_message ("EXISTS", key), read_boolean)
end

function command:sismember(key, value)
	return request(self[1], compose_message ("SISMEMBER", table.pack(key, value)), read_boolean)
end

local function compose_table(lines, msg)
	local tinsert = table.insert
	tinsert(lines, count_cache[#msg])
	for _,v in ipairs(msg) do
		v = tostring(v)
		tinsert(lines,header_cache[#v])
		tinsert(lines,v)
	end
	tinsert(lines, "\r\n")
	return lines
end

function command:pipeline(ops,resp)
	assert(ops and #ops > 0, "pipeline is null")

	local cmds = {}
	for _, cmd in ipairs(ops) do
		compose_table(cmds, cmd)
	end

	if resp then
		return request(self[1], cmds, function(fd)
			for _=1, #ops do
				local ok, out = read_response(fd)
				table.insert(resp, {ok = ok, out = out})
			end
			return true, resp
		end)
	else
		return request(self[1], cmds, function(fd)
			local ok, out
			for _=1, #ops do
				ok, out = read_response(fd)
			end
			-- return last response
			return ok,out
		end)
	end
end

--- watch mode

local watch = {}

local watchmeta = {
	__index = watch,
	__gc = function(self)
		self.__sock:close()
	end,
}

local function watch_login(obj, auth)
	return function(fd)
		if auth then
			request(fd, compose_message("AUTH", auth), read_response)
		end
		for k in pairs(obj.__psubscribe) do
			request(fd, compose_message ("PSUBSCRIBE", k))
		end
		for k in pairs(obj.__subscribe) do
			request(fd, compose_message ("SUBSCRIBE", k))
		end
	end
end

function redis.watch(db_conf)
	local obj = {
		__subscribe = {},
		__psubscribe = {},
	}

	local fd, err = socket.connect(db_conf.host, db_conf.port or 6379, moon.PTYPE_TEXT, db_conf.timeout)
	if not fd then
		return false, err
	end
	socket.setnodelay(fd)

	local f = watch_login(db_conf.auth, db_conf.db)
	if f then
		f(fd)
	end

	obj.__sock = fd

	return setmetatable( obj, watchmeta )
end

function watch:disconnect()
	socket.close(self.__sock)
	setmetatable(self, nil)
end

local function watch_func( name )
	local NAME = string.upper(name)
	watch[name] = function(self, ...)
		local so = self.__sock
		for i = 1, select("#", ...) do
			local v = select(i, ...)
			request(so, compose_message(NAME, v))
		end
	end
end

watch_func "subscribe"
watch_func "psubscribe"
watch_func "unsubscribe"
watch_func "punsubscribe"

function watch:message()
	local so = self.__sock
	while true do
		local ok, ret = read_response(so)
		local ttype , channel, data , data2 = ret[1], ret[2], ret[3], ret[4]
		if ttype == "message" then
			return data, channel
		elseif ttype == "pmessage" then
			return data2, data, channel
		elseif ttype == "subscribe" then
			self.__subscribe[channel] = true
		elseif ttype == "psubscribe" then
			self.__psubscribe[channel] = true
		elseif ttype == "unsubscribe" then
			self.__subscribe[channel] = nil
		elseif ttype == "punsubscribe" then
			self.__psubscribe[channel] = nil
		end
	end
end

return redis

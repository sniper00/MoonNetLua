local moon = require "moon"
local seri = require "seri"
local core = require "sharetable.core"
local fs = require("fs")

local is_sharedtable = core.is_sharedtable
local stackvalues = core.stackvalues

local conf = ...

local function sharetable_service()
	local matrix = {}	-- all the matrix
	local files = {}	-- filename : matrix
	local clients = {}

	local sharetable = {}

	local function close_matrix(m)
		if m == nil then
			return
		end
		local ptr = m:getptr()
		local ref = matrix[ptr]
		if ref == nil or ref.count == 0 then
			matrix[ptr] = nil
			m:close()
		end
	end

    function sharetable.loadfile(source, sessionid, filename, ...)
        if conf.dir then
            filename = fs.join(conf.dir, filename)
        end
		close_matrix(files[filename])
		local m = core.matrix("@" .. filename, ...)
		files[filename] = m
		moon.response("lua", source, sessionid, true)
	end

	function sharetable.loadstring(source, sessionid, filename, datasource, ...)
		close_matrix(files[filename])
		local m = core.matrix(datasource, ...)
		files[filename] = m
		moon.response("lua", source, sessionid)
	end

	local function query_file(source, filename)
		local m = files[filename]
		local ptr = m:getptr()
		local ref = matrix[ptr]
		if ref == nil then
			ref = {
				filename = filename,
				count = 0,
				matrix = m,
				refs = {},
			}
			matrix[ptr] = ref
		end
		if ref.refs[source] == nil then
			ref.refs[source] = true
			local list = clients[source]
			if not list then
				clients[source] = { ptr }
			else
				table.insert(list, ptr)
			end
			ref.count = ref.count + 1
		end
		return ptr
	end

	function sharetable.query(source, sessionid, filename)
        if conf.dir then
            filename = fs.join(conf.dir, filename)
        end
		local m = files[filename]
		if m == nil then
			moon.response("lua", source, sessionid)
			return
		end
		local ptr = query_file(source, filename)
		moon.response("lua", source, sessionid, ptr)
	end

    function sharetable.queryall(source, sessionid)
        local all_ptr = {}
        for filename in pairs(files) do
            local _, name = fs.split(filename)
            all_ptr[name] = query_file(source, filename)
        end
        moon.response("lua", source, sessionid, all_ptr)
    end

	function sharetable.close(source)
		local list = clients[source]
		if list then
			for _, ptr in ipairs(list) do
				local ref = matrix[ptr]
				if ref and ref.refs[source] then
					ref.refs[source] = nil
					ref.count = ref.count - 1
					if ref.count == 0 then
						if files[ref.filename] ~= ref.matrix then
							-- It's a history version
							moon.warn(string.format("Delete a version (%s) of %s", ptr, ref.filename))
							ref.matrix:close()
							matrix[ptr] = nil
						end
					end
				end
			end
			clients[source] = nil
		end
		-- no return
	end

	local unpack = seri.unpack
	local unpack_one = seri.unpack_one

    moon.dispatch("lua",function(msg)
        local sender, sessionid, buf = moon.decode(msg, "SEB")
        local cmd, sz, len = unpack_one(buf)
		sharetable[cmd](sender, sessionid, unpack(sz, len))
	end)

    ---sharetable service's 'files' use fs.join(conf.dir, filename)
	if conf.dir then
        local list = fs.listdir(conf.dir, 0,".lua")
        for _,file in ipairs(list) do
            local _, name = fs.split(file)
            sharetable.loadfile(0,0, name)
        end
	end
end

if conf and conf.name then
	sharetable_service()
	return
end

local function load_service(t, key)
	if key == "address" then
		t.address = moon.queryservice("sharetable")
		return t.address
	else
		return nil
	end
end

local function report_close(t)
	local addr = rawget(t, "address")
    if addr then
        moon.send("lua", addr, "close")
	end
end

local sharetable =  setmetatable ( {} , {
    __index = load_service,
	__gc = report_close,
})

function sharetable.loadfile(filename, ...)
	return moon.co_call( "lua", sharetable.address, "loadfile", filename, ...)
end

function sharetable.loadstring(filename, source, ...)
	return moon.co_call( "lua", sharetable.address, "loadstring", filename, source, ...)
end

local RECORD = {}
---filename: xxx.lua
function sharetable.query(filename)
	local newptr, err = moon.co_call( "lua", sharetable.address, "query", filename)
	if newptr then
		local t = core.clone(newptr)
		local map = RECORD[filename]
		if not map then
			map = {}
			RECORD[filename] = map
		end
		map[t] = true
		return t
    end
    return newptr, err
end

function sharetable.queryall()
    local conf_ptr,err = moon.co_call( "lua", sharetable.address, "queryall")
    assert(conf_ptr,err)
    local tbconf = {}
    for filename, newptr in pairs(conf_ptr) do
        local t = core.clone(newptr)
		local map = RECORD[filename]
		if not map then
			map = {}
			RECORD[filename] = map
		end
        map[t] = true
        tbconf[fs.stem(filename)] = t
    end
    return tbconf
end

local pairs = pairs
local type = type
local assert = assert
local next = next
local rawset = rawset
local getuservalue = debug.getuservalue
local setuservalue = debug.setuservalue
local getupvalue = debug.getupvalue
local setupvalue = debug.setupvalue
local getlocal = debug.getlocal
local setlocal = debug.setlocal
local getinfo = debug.getinfo

local NILOBJ = {}
local function insert_replace(old_t, new_t, replace_map)
    for k, ov in pairs(old_t) do
        if type(ov) == "table" then
            local nv = new_t[k]
            if nv == nil then
                nv = NILOBJ
            end
            assert(replace_map[ov] == nil)
            replace_map[ov] = nv
            nv = type(nv) == "table" and nv or NILOBJ
            insert_replace(ov, nv, replace_map)
        end
    end
    replace_map[old_t] = new_t
    return replace_map
end


local function resolve_replace(replace_map)
    local match = {}
    local record_map = {}

    local function getnv(v)
        local nv = replace_map[v]
        if nv then
            if nv == NILOBJ then
                return nil
            end
            return nv
        end
        assert(false)
    end

    local function match_value(v)
        if v == nil or record_map[v] or is_sharedtable(v) then
            return
        end

        local tv = type(v)
        local f = match[tv]
        if f then
            record_map[v] = true
            return f(v)
        end
    end

    local function match_mt(v)
        local mt = debug.getmetatable(v)
        if mt then
            local nv = replace_map[mt]
            if nv then
                nv = getnv(mt)
                debug.setmetatable(v, nv)
            else
                return match_value(mt)
            end
        end
    end

    local function match_internmt()
        local internal_types = {
            pointer = debug.upvalueid(getnv, 1),
            boolean = false,
            str = "",
            number = 42,
            thread = coroutine.running(),
            func = getnv,
        }
        for _,v in pairs(internal_types) do
            match_mt(v)
        end
        return match_mt(nil)
    end


    local function match_table(t)
        local keys = false
        for k,v in next, t do
            local tk = type(k)
            if match[tk] then
                keys = keys or {}
                keys[#keys+1] = k
            end

            local nv = replace_map[v]
            if nv then
                nv = getnv(v)
                rawset(t, k, nv)
            else
                match_value(v)
            end
        end

        if keys then
            for _, old_k in ipairs(keys) do
                local new_k = replace_map[old_k]
                if new_k then
                    local value = rawget(t, old_k)
                    new_k = getnv(old_k)
                    rawset(t, old_k, nil)
                    if new_k then
                        rawset(t, new_k, value)
                    end
                else
                    match_value(old_k)
                end
            end
        end
        return match_mt(t)
    end

    local function match_userdata(u)
        local uv = getuservalue(u)
        local nv = replace_map[uv]
        if nv then
            nv = getnv(uv)
            setuservalue(u, nv)
        end
        return match_mt(u)
    end

    local function match_funcinfo(info)
        local func = info.func
        local nups = info.nups
        for i=1,nups do
            local name, upv = getupvalue(func, i)
            local nv = replace_map[upv]
            if nv then
                nv = getnv(upv)
                setupvalue(func, i, nv)
            elseif upv then
                match_value(upv)
            end
        end

        local level = info.level
        local curco = info.curco
        if not level then
            return
        end
        local i = 1
        while true do
            local name, v = getlocal(curco, level, i)
            if name == nil then
                break
            end
            if replace_map[v] then
                local nv = getnv(v)
                setlocal(curco, level, i, nv)
            elseif v then
                match_value(v)
            end
            i = i + 1
        end
    end

    local function match_function(f)
        local info = getinfo(f, "uf")
        return match_funcinfo(info)
    end

    local function match_thread(co, level)
        -- match stackvalues
        local values = {}
        local n = stackvalues(co, values)
        for i=1,n do
            local v = values[i]
            match_value(v)
        end

        local uplevel = co == coroutine.running() and 1 or 0
        level = level or 1
        while true do
            local info = getinfo(co, level, "uf")
            if not info then
                break
            end
            info.level = level + uplevel
            info.curco = co
            match_funcinfo(info)
            level = level + 1
        end
    end

    local function prepare_match()
        local co = coroutine.running()
        record_map[co] = true
        record_map[match] = true
        record_map[RECORD] = true
        record_map[record_map] = true
        record_map[replace_map] = true
        record_map[insert_replace] = true
        record_map[resolve_replace] = true
        assert(getinfo(co, 3, "f").func == sharetable.update)
        match_thread(co, 5) -- ignore match_thread and match_funcinfo frame
    end

    match["table"] = match_table
    match["function"] = match_function
    match["userdata"] = match_userdata
    match["thread"] = match_thread

    prepare_match()
    match_internmt()

    local root = debug.getregistry()
    assert(replace_map[root] == nil)
    match_table(root)
end


function sharetable.update(...)
	local names = {...}
	local replace_map = {}
	for _, name in ipairs(names) do
		local map = RECORD[name]
		if map then
			local new_t = sharetable.query(name)
			for old_t,_ in pairs(map) do
				if old_t ~= new_t then
					insert_replace(old_t, new_t, replace_map)
                    map[old_t] = nil
				end
			end
		end
	end

    if next(replace_map) then
        resolve_replace(replace_map)
    end
end

return sharetable


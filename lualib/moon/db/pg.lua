local moon = require("moon")
local seri = require ("seri")
local cbuffer = require("buffer")
local socket = require("moon.socket")

local concat = seri.concat
local bsize = cbuffer.size
local bwritefront = cbuffer.write_front
local bwrite = cbuffer.write_back

local socket_error = setmetatable({}, {__tostring = function() return "[Error: socket]" end })	-- alias for error object

local md5 = moon.md5

local NULL = "\000"

local insert = table.insert

local strpack = string.pack
local strunpack = string.unpack
local strsub = string.sub

local type = type
local assert = assert

local tointeger = math.tointeger
local tonumber = tonumber

local convert_null = true

local flipped
flipped = function(t)
    local keys
    do
        local _accum_0 = {}
        local _len_0 = 1
        for k in pairs(t) do
            _accum_0[_len_0] = k
            _len_0 = _len_0 + 1
        end
        keys = _accum_0
    end
    for _index_0 = 1, #keys do
        local key = keys[_index_0]
        t[t[key]] = key
    end
    return t
end

local MSG_TYPE =
    flipped(
    {
        status = "S",
        auth = "R",
        backend_key = "K",
        ready_for_query = "Z",
        query = "Q",
        notice = "N",
        notification = "A",
        password = "p",
        row_description = "T",
        data_row = "D",
        command_complete = "C",
        error = "E"
    }
)

local ERROR_TYPES =
    flipped(
    {
        severity = "S",
        code = "C",
        message = "M",
        position = "P",
        detail = "D",
        schema = "s",
        table = "t",
        constraint = "n"
    }
)

local PG_TYPES = {
    ---"boolean"
    [16] =  function(value)
        return value == "t"
    end,
    -- [17] = "bytea",
    [20] = tointeger,
    [21] = tointeger,
    [23] = tointeger,
    [700] = tonumber,
    [701] = tonumber,
    [1700] = tonumber,
    -- [114] = "json",
    -- [3802] = "json",
    -- [1000] = "array_boolean",
    -- [1005] = "array_number",
    -- [1007] = "array_number",
    -- [1016] = "array_number",
    -- [1021] = "array_number",
    -- [1022] = "array_number",
    -- [1231] = "array_number",
    -- [1009] = "array_string",
    -- [1015] = "array_string",
    -- [1002] = "array_string",
    -- [1014] = "array_string",
    -- [2951] = "array_string",
    -- [199] = "array_json",
    -- [3807] = "array_json"
}

local function encode_int(n)
    return strpack(">i", n)
end

local function decode_int(s)
    return strunpack(">i", s)
end

local function decode_short(s)
    return strunpack(">H", s)
end

local function disconnect(self)
    if self.sock then
        socket.close(self.sock)
        self.sock = nil
    end
end

local function receive_message(self)
    if not self.sock then
        return socket_error,"not connect"
    end
    local header, err, content
    ---header: 1byte type + 4byte len
    header, err = socket.read(self.sock, 5)
    if not header then
        disconnect(self)
        return socket_error, "receive_message: failed to get header: " .. tostring(err)
    end
    local t = strsub(header, 1, 1)
    local len = strunpack(">i", header, 2) - 4
    content, err = socket.read(self.sock, len)
    if not content then
        disconnect(self)
        return socket_error, err
    end
    return t, content
end

local function send_message(self, t, data)
    local buf = concat(data)
    local len = bsize(buf)
    bwritefront(buf, strpack(">I", len+4))
    bwritefront(buf, t)
    socket.write(self.sock, buf)
end

local function send_startup_message(self)
    assert(self.user, "missing user for connect")
    assert(self.database, "missing database for connect")
    local data = {
        encode_int(196608),
        "user",
        NULL,
        self.user,
        NULL,
        "database",
        NULL,
        self.database,
        NULL,
        "application_name",
        NULL,
        "moon",
        NULL,
        NULL
    }

    local str = seri.concats(data)

    socket.write(self.sock, seri.concats(encode_int(#str + 4), data))
end

local function parse_error(err_msg)
	local db_error = { }
	local offset = 1
	while offset <= #err_msg do
		local t = err_msg:sub(offset, offset)
		local str = err_msg:match("[^%z]+", offset + 1)
		if not (str) then
			break
		end
		offset = offset + (2 + #str)
		local field = ERROR_TYPES[t]
		if field then
			db_error[field] = str
		end
	end
    return db_error
end

local function check_auth(self)
    local t, msg = receive_message(self)
    if t == socket_error then
        return socket_error, msg
    end
    local _exp_0 = t
    if MSG_TYPE.error == _exp_0 then
        return false, parse_error(msg)
    elseif MSG_TYPE.auth == _exp_0 then
        return true
    else
        return error("unknown response from auth")
    end
end

local function cleartext_auth(self)
    assert(self.password, "missing password, required for connect")
    send_message(
        self,
        MSG_TYPE.password,
        {
            self.password,
            NULL
        }
    )
    return check_auth(self)
end

local function md5_auth(self, msg)
    local salt = msg:sub(5, 8)
    assert(self.password, "missing password, required for connect")
    send_message(
        self,
        MSG_TYPE.password,
        {
            "md5",
            md5(md5(self.password .. self.user) .. salt),
            NULL
        }
    )
    return check_auth(self)
end

local function auth(self)
    local t, msg = receive_message(self)
    if t == socket_error then
        return socket_error, msg
    end
    if not (MSG_TYPE.auth == t) then
        disconnect(self)
        if MSG_TYPE.error == t then
            return false, parse_error(msg)
        end
        error("unexpected message during auth: " .. tostring(t))
    end
    local auth_type = strunpack(">i", msg)
    local _exp_0 = auth_type
    if 0 == _exp_0 then
        return true
    elseif 3 == _exp_0 then
        return cleartext_auth(self)
    elseif 5 == _exp_0 then
        return md5_auth(self, msg)
    else
        return error("don't know how to auth: " .. tostring(auth_type))
    end
end

local function wait_until_ready(self)
    while true do
        local t, msg = receive_message(self)
        if t == socket_error then
            return t, msg
        end
        if MSG_TYPE.error == t then
            disconnect(self)
            return false, parse_error(msg)
        end
		if MSG_TYPE.ready_for_query == t then
            break
        end
	end
    return true
end

---@class pg
local pg = {}

pg.disconnect = disconnect

pg.__index = pg

pg.__gc = function(o)
    if o.sock then
        socket.close(o.sock)
    end
end

pg.socket_errcode = -1

---@param opts table @{database = "", user = "", password = ""}
---@return pg
function pg.connect(opts)
    local sock, err = socket.connect(opts.host, opts.port, moon.PTYPE_TEXT, opts.connect_timeout)
    if not sock then
        return {code= pg.socket_errcode, message = err}
    end

    local obj = table.deepcopy(opts)
    obj.sock = sock

    send_startup_message(obj)

    local success

    success, err = auth(obj)
    if success == socket_error then
        return {code= pg.socket_errcode, message = err}
    end

    if not success then
        return err
    end

    success, err = wait_until_ready(obj)
    if success == socket_error then
        return {code= pg.socket_errcode, message = err}
    end

    if not success then
        return err
    end

    return setmetatable(obj, pg)
end

-- local json = require("json")


-- local type_deserializers = {
--     json = json.decode,
--     bytea = function(val)
--         return self:decode_bytea(val)
--     end,
--     array_boolean = function(self, val, name)
--         local decode_array
--         decode_array = require("pgmoon.arrays").decode_array
--         return decode_array(val, tobool)
--     end,
--     array_number = function(self, val, name)
--         local decode_array
--         decode_array = require("pgmoon.arrays").decode_array
--         return decode_array(val, tonumber)
--     end,
--     array_string = function(self, val, name)
--         local decode_array
--         decode_array = require("pgmoon.arrays").decode_array
--         return decode_array(val)
--     end,
--     array_json = function(self, val, name)
--         local decode_array
--         decode_array = require("pgmoon.arrays").decode_array
--         local decode_json
--         decode_json = require("pgmoon.json").decode_json
--         return decode_array(val, decode_json)
--     end,
--     hstore = function(self, val, name)
--         local decode_hstore
--         decode_hstore = require("pgmoon.hstore").decode_hstore
--         return decode_hstore(val)
--     end
-- }

local function parse_row_desc(row_desc)
    local num_fields = decode_short(row_desc:sub(1, 2))
    local offset = 3
    local fields = {}
    for _ = 1, num_fields do
        local name = row_desc:match("[^%z]+", offset)
        offset = offset + #name + 1
        local data_type = decode_int(row_desc:sub(offset + 6, offset + 9))
        --data_type = PG_TYPES[data_type] or "string"
        local format = decode_short(row_desc:sub(offset + 16, offset + 17))
        assert(0 == format, "don't know how to handle format")
        offset = offset + 18
        local info = {
            name,
            data_type
        }
        table.insert(fields, info)
    end
    return fields
end



local function parse_row_data(data_row, fields)
    local tupnfields = decode_short(data_row:sub(1, 2))
    assert(tupnfields == #fields, 'unexpected field count in \"D\" message')
    local out = {}
    local offset = 3
    for i = 1, tupnfields do
        local field = fields[i]
        local field_name, field_type = field[1], field[2]
        local len = decode_int(data_row:sub(offset, offset + 3))
        offset = offset + 4
        if len < 0 then
            if convert_null then
                out[field_name] = NULL
            end
        else
            local value = data_row:sub(offset, offset + len - 1)
            offset = offset + len
            local fn = PG_TYPES[field_type]
            if fn then
                value = fn(value)
            end
            out[field_name] = value
        end
    end
    return out
end

local function parse_notification(msg)
    local pid = decode_int(msg:sub(1, 4))
    local offset = 4
    local channel, payload = msg:match("^([^%z]+)%z([^%z]*)%z$", offset + 1)
    if not (channel) then
        error("parse_notification: failed to parse notification")
    end
    return {
        operation = "notification",
        pid = pid,
        channel = channel,
        payload = payload
    }
end

local function format_query_result(row_desc, data_rows, command_complete)
    local command, affected_rows
    if command_complete then
        command = command_complete:match("^%w+")
        affected_rows = tointeger(command_complete:match("(%d+)%z$"))
    end
    if row_desc then
        if not (data_rows) then
            return {}
        end
        local fields = parse_row_desc(row_desc)
        local num_rows = #data_rows
        for i = 1, num_rows do
            data_rows[i] = parse_row_data(data_rows[i], fields)
        end
        if affected_rows and command ~= "SELECT" then
            data_rows.affected_rows = affected_rows
        end
        return data_rows
    end
    if affected_rows then
        return {
            affected_rows = affected_rows
        }
    else
        return true
    end
end

function pg.pack_query_buffer(buf)
    bwrite(buf, "\0")
    local len = bsize(buf)
    bwritefront(buf, strpack(">I", len+4))
    bwritefront(buf, MSG_TYPE.query)
end

---@class pg_result
---@field public data table @ table rows
---@field public num_queries integer
---@field public notifications integer
---@field public errmsg string @ error message
---@field public errdata table @ error detail info


---@param sql userdata|string @ userdata cpp message pointer:sql string
---@return pg_result
function pg.query(self, sql)
    if type(sql) == "string" then
        send_message(self, MSG_TYPE.query, {sql, NULL})
    else
        socket.write_message(self.sock, sql)
    end
    local row_desc, data_rows, err_msg
    local result, notifications
    local num_queries = 0
    while true do
        local t, msg = receive_message(self)
        if t == socket_error then
            return {code= pg.socket_errcode, message = msg}
        end
        local _exp_0 = t
        if MSG_TYPE.data_row == _exp_0 then
            data_rows = data_rows or {}
            insert(data_rows, msg)
        elseif MSG_TYPE.row_description == _exp_0 then
            row_desc = msg
        elseif MSG_TYPE.error == _exp_0 then
            err_msg = msg
        elseif MSG_TYPE.command_complete == _exp_0 then

            local next_result = format_query_result(row_desc, data_rows, msg)
            num_queries = num_queries + 1
            if num_queries == 1 then
                result = next_result
            elseif num_queries == 2 then
                result = {
                    result,
                    next_result
                }
            else
                insert(result, next_result)
            end
            row_desc = nil
            data_rows = nil
        elseif MSG_TYPE.ready_for_query == _exp_0 then
            break
        elseif MSG_TYPE.notification == _exp_0 then
            if not (notifications) then
                notifications = {}
            end
            insert(notifications, parse_notification(msg))
        end
    end
    if err_msg then
        local db_error = parse_error(err_msg)
        db_error.data = result
        db_error.num_queries = num_queries
        db_error.notifications = notifications
        return db_error
    end

    return {
        data = result,
        num_queries = num_queries,
        notifications = notifications
    }
end

return pg
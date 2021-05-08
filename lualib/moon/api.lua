---@class core
---@field public timezone integer @当前时区
local core = {}

--------------------FOR EMMYLUA HINT---------------------

--- ignore warning: unused params
local ignore_param = function (...) end

---service's name
core.name = ""

---service's id
core.id = 0

---timezone
core.timezone = 0

---return system microsecond.
---@return integer
function core.microseconds()
    -- body
end

---string's md5.
---@param data string
---@return string
function core.md5(data)
    ignore_param(data)
end

---convert c-string(char* and size) to lua-string
---@param sz userdata @ lightuserdata char*
---@param len integer
---@return string
function core.tostring(sz, len)
    ignore_param(sz, len)
end

---get tm
---@param t integer@ utc time
---@return tm
function core.localtime(t)
    ignore_param(t)
end

---create a timer
---@param interval integer @ms
---@return integer @timer id
function core.timeout(interval)
    ignore_param(interval)
end

--- print console log
function core.log(loglv,...)
    ignore_param(loglv, ...)
end

--- set log level
---@param lv string @DEBUG, INFO, WARN, ERROR
---@return integer
function core.set_loglevel(lv)
    ignore_param(lv)
end

--- get log level
---@return integer
function core.get_loglevel()
end

--- get this service's cpu cost time
---@return integer
function core.cpu()
    -- body
end

---make a prefab message
---@param data string|userdata
---@return integer @prefab id
function core.make_prefab(data)
    ignore_param(data)
end

---send prefab message
---@param receiver integer
---@param prefabid integer
---@param header string
---@param sessionid integer
---@param type integer
function core.send_prefab(receiver, prefabid, header, sessionid, type)
    ignore_param(receiver, prefabid, header, sessionid, type)
end

---向目标服务发送消息
---@param sender integer
---@param receiver integer
---@param data string|userdata
---@param header string
---@param sessionid integer
function core.send(sender, receiver, data, header, sessionid)
    ignore_param(sender, receiver, data, header, sessionid)
end

--- remove a service
function core.kill(addr, sessionid)
    ignore_param(addr, sessionid)
end

--- use for query framework info
---@param sender integer @
---@param cmd string
---@param sessionid integer @
---@return string
function core.runcmd(sender, cmd, sessionid)
    ignore_param(sender, cmd, sessionid)
end

--- 根据服务名字查询服务ID
function core.queryservice(name)
    ignore_param(name)
end

---@param key string
---@param value string
function core.set_env(key, value)
    ignore_param(key, value)
end

---@param key string
---@return string
function core.get_env(key)
    ignore_param(key)
end

---get worker thread info
---@return string
function core.wsate(workerid)
    ignore_param(workerid)
end

---发送进程退出信号
---@param exitcode integer
function core.exit(exitcode)
    ignore_param(exitcode)
end

--- get total services's count()
---@return integer
function core.size()
    -- body
end

---推进时间 server time(millsecond)
---@param milliseconds integer
function core.adjtime(milliseconds)
    ignore_param(milliseconds)
end

--- set lua callback
---@param fn fun(msg:userdata,ptype:integer)
function core.callback(fn)
    ignore_param(fn)
end

--- get server time(milliseconds)
---@return integer
function core.now()
    -- body
end

---获取message相关信息
---'S' message:sender()
---
---'R' message:receiver()
---
---'E' message:sessionid()
---
---'H' message:header()
---
---'Z' message:bytes()
---
---'N' message:size()
---
---'B' message:buffer()
---
---'C' message:buffer():data() message:buffer():size()
---@param msg userdata @message* lightuserdata
---@param pattern string
function core.decode(msg, pattern)
    ignore_param(msg, pattern)
end

---clone message, but share buffer field
---@param msg userdata @ message* lightuserdata
---@return userdata
function core.clone(msg)
    ignore_param(msg)
end

---release clone message
---@param msg userdata @ message* lightuserdata
function core.release(msg)
    ignore_param(msg)
end

---redirect a message to other service
function core.redirect(header, receiver, mtype, sender, sessionid)
    ignore_param(header, receiver, mtype, sender, sessionid)
end

---@class fs
local fs = {}
ignore_param(fs)

---返回指定的文件夹包含的文件或文件夹的名字的列表
---@param dir string @路径
---@param depth integer @遍历子目录的深度，默认0
---@return table @返回数组
function fs.listdir(dir, depth)
    ignore_param(dir, depth)
end

function fs.isdir(dir)
    ignore_param(dir)
end

function fs.join(dir1, dir2)
    ignore_param(dir1, dir2)
end

---@param dir string
function fs.exists(dir)
    ignore_param(dir)
end

---@param dir string
function fs.mkdir(dir)
    ignore_param(dir)
end

---@param fp string
---@param all boolean
function fs.remove(fp, all)
    ignore_param(fp, all)
end

---@return string
function fs.cwd()
    -- body
end

---@param fp string
---@return string,string @ 返回dirname, basename
function fs.split(fp)
    ignore_param(fp)
end

---@param fp string
---@return string
function fs.ext(fp)
    ignore_param(fp)
end

---@param fp string
---@return string
function fs.root(fp)
    ignore_param(fp)
end

---@param fp string
---@return string
function fs.stem(fp)
    ignore_param(fp)
end

---@param fp string
---@return string
function fs.abspath(fp)
    ignore_param(fp)
end

---@class asio
local asio = {}
ignore_param(asio)

---@param host string
---@param port integer
function asio.try_open(host, port)
    ignore_param(host, port)
end

---param protocol moon.PTYPE_TEXT、moon.PTYPE_SOCKET、moon.PTYPE_SOCKET_WS、
---@param host string
---@param port integer
---@param protocol integer
function asio.listen(host, port, protocol)
    ignore_param(host, port, protocol)
end

---send data to fd, data string or userdata moon.buffer*
---@param fd integer
---@param data string|userdata
---@param flag integer|nil
---@return boolean
function asio.write(fd, data, flag)
    ignore_param(fd, data, flag)
end

---@param fd integer
---@param m userdata @ message*
---@return boolean
function asio.write_message(fd, m)
    ignore_param(fd, m)
end

---@param fd integer
---@param t integer 秒, 0不检测超时, 默认是0。
---@return boolean
function asio.settimeout(fd, t)
    ignore_param(fd, t)
end

---@param fd integer
---@return boolean
function asio.setnodelay(fd)
    ignore_param(fd)
end

---@param fd integer
---@param flag string
---对于2字节大端长度开头的协议, 通过拆分，来支持收发超过2字节的数据。可以单独控制read,write "r", "w", "wr"
---@return boolean
function asio.set_enable_chunked(fd, flag)
    ignore_param(fd,flag)
end

---@param fd integer
function asio.close(fd)
    ignore_param(fd)
end

---@param fd integer
---@return string @ format ip:port
function asio.getaddress(fd)
    ignore_param(fd)
end

---@class random
local random = {}
ignore_param(random)

---integer rand range [min,max]
---@param min integer
---@param max integer
---@return integer
function random.rand_range(min, max)
    ignore_param(min, max)
end

---integer rand range [min,max],返回范围内指定个数不重复的随机数
---@param min integer
---@param max integer
---@param count integer
---@return table
function random.rand_range_some(min, max, count)
    ignore_param(min, max, count)
end

---double rand range [min,max)
---@param min number
---@param max number
---@return number
function random.randf_range(min, max)
    ignore_param(min, max)
end

---参数 percent (0,1.0),返回bool
---@param percent number
---@return boolean
function random.randf_percent(percent)
    ignore_param(percent)
end

---按权重随机
---@param values integer[] @要随机的值
---@param weights integer[] @要随机的值的权重
---@return integer
function random.rand_weight(values, weights)
    ignore_param(values, weights)
end

---按权重随机count个值
---@param values integer[] @要随机的值
---@param weights integer[] @要随机的值的权重
---@param count integer @要随机的个数
---@return table
function random.rand_weight_some(values, weights, count)
    ignore_param(values, weights, count)
end

---@class ranklist
local ranklist = {}
ignore_param(ranklist)

---创建ranklist对象
---maxlen > 0 则ranklist榜单最多包含maxlen个元素, 如果为-1则榜单元素无上限
---@param maxlen integer
function ranklist.new(maxlen)
    ignore_param(maxlen)
end

---更新分数 按照唯一ID 分数 时间 传参 返回名次
---@param id integer
---@param score integer
---@param time integer
---@return integer
function ranklist:update(id, score, time)
    ignore_param(self, id, score, time)
end

---根据唯一ID获取名次 返回0代表未上榜
---@param uniqueid integer
---@return integer
function ranklist:rank(uniqueid)
    ignore_param(self, uniqueid)
end

---获取排行榜长度
---@return integer
function ranklist:size()
    ignore_param(self)
end

---清除排行榜
function ranklist:clear()
    ignore_param(self)
end

---根据名次获取唯一ID
---@param rank integer
---@return integer
function ranklist:key(rank)
    ignore_param(self, rank)
end

---查询uniqueid的积分
---@param uniqueid integer
---@return integer
function ranklist:score(uniqueid)
    ignore_param(self, uniqueid)
end

---获取[start, finish]名次的唯一ID列表
---@param start integer
---@param finish integer
---@return table
function ranklist:range(start, finish)
    ignore_param(self, start, finish)
end

---打印[start, finsih]名次的信息
---@param start integer
---@param finish integer
function ranklist:print(start, finish)
    ignore_param(self, start, finish)
end

---设置排行榜的score越小排行越高,默认score越大排行越高
function ranklist:set_small_better()
    ignore_param(self)
end

---@class uuid
local uuid = {}
ignore_param(uuid)

---初始化,只用调用一次
---@param serverid integer
---@param server_start_times integer
---@param servergroup integer
---@param region integer
function uuid.init(serverid, server_start_times, servergroup, region)
    ignore_param(serverid,server_start_times, servergroup, region)
end

---生成 int64 uuid,可以传入类型，最大值 1023
---@param type_ integer
---@return integer
function uuid.next(type_)
    ignore_param(type_)
end

---获取uuid的 type
---@param uuidvalue integer
---@return integer
function uuid.type(uuidvalue)
    ignore_param(uuidvalue)
end

---生成 int64 player uid
---@return integer
function uuid.allocuid()
end

return core

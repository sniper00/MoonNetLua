local moon = require("moon")
local coroutine = coroutine
local table = table
local traceback = debug.traceback

local function queue()
	local current_thread
	local ref = 0
	local thread_queue = {}

	local scope = setmetatable({}, { __close = function()
		ref = ref - 1
		if ref == 0 then
			current_thread = table.remove(thread_queue,1)
			if current_thread then
				moon.timeout(0, function()
					local ok, err = coroutine.resume(current_thread)
					if not ok then
						err = traceback(current_thread, tostring(err))
						coroutine.close(current_thread)
						moon.error(err)
					end
				end)
			end
		end
	end})

	return function()
		local thread = coroutine.running()
		if current_thread and current_thread ~= thread then
			table.insert(thread_queue, thread)
			coroutine.yield()
			assert(ref == 0)	-- current_thread == thread
		end
		current_thread = thread
		ref = ref + 1
		return scope
	end
end

return queue

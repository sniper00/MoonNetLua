local moon = require("moon")
local conf = ...
local reload = require "hardreload"
local require = reload.require
-- reload.postfix = "_update"	-- for test

---hot fix example

reload.addsearcher(function(name)
	local file, err = io.open(name..".lua", "rb")
	if file then
        local content = file:read("*a")
        io.close(file)
		return load(content,"@"..name),name
    end
    error(err)
end)

local reload_module = require("reload_before")

moon.async(function ()
    local rmd = reload_module.new()
    while true do
        rmd:func()
    end
end)

print("10 seconds later hot fix")
moon.timeout(10000, function (  )
    print("Hot Fix Result",reload.reload("reload_before", "reload_after"))
end)



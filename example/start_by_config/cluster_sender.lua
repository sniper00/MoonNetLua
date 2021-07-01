local moon = require("moon")
local cluster = require("cluster")

local counter = 0

moon.async(function()
    while true do
        moon.sleep(1000)
        print("per sec ",counter)
        counter=0
    end
end)

local args = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}
moon.async(function()
    print(moon.co_call("lua", moon.queryservice("cluster"), "Start"))
    cluster.call(9, 'cluster_receiver', "ACCUM", table.unpack(args))
    for i=1,100000 do
        cluster.send(9, 'cluster_receiver',"COUNTER", moon.now())
    end

    while true do
        local ret ,err = cluster.call(9, 'cluster_receiver', "ACCUM", table.unpack(args))
        if not ret then
            print(err)
            return
        end
        counter=counter+1
    end
end)








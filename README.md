# Current building status

[![linux-gcc](https://github.com/sniper00/moon/actions/workflows/linux-gcc.yml/badge.svg)](https://github.com/sniper00/moon/actions/workflows/linux-gcc.yml)

[![linux-clang](https://github.com/sniper00/moon/actions/workflows/linux-clang.yml/badge.svg)](https://github.com/sniper00/moon/actions/workflows/linux-clang.yml)

[![macos-clang](https://github.com/sniper00/moon/actions/workflows/macosx-clang.yml/badge.svg)](https://github.com/sniper00/moon/actions/workflows/macosx-clang.yml)

[![windows-vs2019](https://github.com/sniper00/moon/actions/workflows/windows-vs2019.yml/badge.svg)](https://github.com/sniper00/moon/actions/workflows/windows-vs2019.yml)

# Moon
Moon is a lightweight online game server framework implement with multithread and multi-luaVM. One thread may have 1-N luaVM, they use message queue communication. There are many features for game server development:

- Cross-platform (Windows, Linux, MacOS)
- Optimized networking
   - tcp
   - websockets
   - http
- Lua coroutine based asynchronous
   - coroutine-socket
   - timer
   - inter luaVM communication
   - inter cluster(process) communication
   - redis/mysql/pgsql driver
- High performance and optimized LuaJSON library
- Lua protobuf library
- Lua filesystem(C++ std::filesystem)

# Community

- QQ group: 543833695

# Documents
  
  https://github.com/sniper00/moon/wiki

# Dependencies

- [asio](https://github.com/chriskohlhoff/asio)
- [lua](https://github.com/cloudwu/skynet/tree/master/3rd/lua)
- [rapidjson](https://github.com/Tencent/rapidjson)

# Quick Start

1. [Install `C++17` Compiler.](https://github.com/sniper00/moon/wiki/Build#%E5%AE%89%E8%A3%85c17%E7%BC%96%E8%AF%91%E5%99%A8)。

2. Clone source code
```
    git clone https://github.com/sniper00/moon.git
``` 

3. Build
    - windows run `build.bat`。
    - linux:
        ```shell
            chmod +x build.sh
            chmod +x premake5
            ./build.sh
        ```

    **If failed，[See detail steps](https://github.com/sniper00/moon/wiki/Build#%E7%BC%96%E8%AF%91)**。

4. Run

```shell
# run echo server
./moon example/helloworld.lua
# another terminal run client(coroutine socket writed)
./moon example/helloworld_client.lua
# input any string
```

## Demo
- [BallGame](https://github.com/sniper00/BallGame.git)

# Friend Open Source
**NoahGameFrame**  
-  Author: ketoo  
-  GitHub: https://github.com/ketoo/NoahGameFrame  
-  Description: A fast, scalable, distributed game server framework for C++, include actor library, network library,can be used as a real time multiplayer game engine ( MMO RPG ), which plan to support C#/Python/Lua script, and support Unity3d, Cocos2dx, FlashAir client access.  

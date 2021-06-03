#include <csignal>
#include "common/log.hpp"
#include "common/directory.hpp"
#include "common/string.hpp"
#include "common/file.hpp"
#include "common/lua_utility.hpp"
#include "server.h"
#include "services/lua_service.h"

extern "C" {
#include "lstring.h"
}

static std::weak_ptr<moon::server>  wk_server;

#if TARGET_PLATFORM == PLATFORM_WINDOWS
static BOOL WINAPI ConsoleHandlerRoutine(DWORD dwCtrlType)
{
    auto svr = wk_server.lock();
    if (nullptr == svr)
    {
        return TRUE;
    }

    switch (dwCtrlType)
    {
    case CTRL_C_EVENT:
        svr->stop(100+ dwCtrlType);
        return TRUE;
    case CTRL_CLOSE_EVENT:
    case CTRL_SHUTDOWN_EVENT:
    case CTRL_LOGOFF_EVENT://atmost 10 second,will force closed by system
        svr->stop(100 + dwCtrlType);
        while (svr->get_state() != moon::state::stopped)
        {
            std::this_thread::yield();
        }
        return TRUE;
    default:
        break;
    }
    return FALSE;
}
#else
static void signal_handler(int signal)
{
    auto svr = wk_server.lock();
    if (nullptr == svr)
    {
        return;
    }

    std::string_view msg;
    switch (signal)
    {
    case SIGTERM:
        msg = "Received SIGTERM,shutdown...\n";
        break;
    case SIGINT:
        msg = "Received SIGINT,shutdown...\n";
        break;
    default:
        msg = "Received shutdown signal,shutdown...\n";
        break;
    }
    svr->stop(signal);
    [[maybe_unused]] auto n = write(STDERR_FILENO, msg.data(), msg.size());
}
#endif

static void register_signal(int argc, char* argv[])
{
#if TARGET_PLATFORM == PLATFORM_WINDOWS
    SetConsoleCtrlHandler(ConsoleHandlerRoutine, TRUE);
    std::string str;
    for (int i = 0; i < argc; ++i)
    {
        str.append(argv[i]);
        if (i == 0)
        {
            str.append("(PID: ");
            str.append(std::to_string(GetCurrentProcessId()));
            str.append(")");
        }
        str.append(" ");
    }
    SetConsoleTitle(str.data());
#else
    std::signal(SIGHUP, SIG_IGN);
    std::signal(SIGQUIT, SIG_IGN);
    std::signal(SIGPIPE, SIG_IGN);
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
#endif
}

#ifdef MOON_ENABLE_MIMALLOC
#include "mimalloc.h"
void print_mem_stats()
{
    mi_collect(true);
    mi_stats_print(nullptr);
}
#else
void print_mem_stats()
{
}
#endif

static void usage(void) {
    std::cout << "Usage:\n";
    std::cout << "        moon script [args]\n";
    std::cout << "Examples:\n";
    std::cout << "        moon main.lua  hello\n";
}

int main(int argc, char* argv[])
{
    using namespace moon;

    time::timezone();

    register_signal(argc, argv);

#ifdef LUA_CACHELIB
    luaL_initcodecache();
#endif
    try
    {
        uint32_t thread_count = std::thread::hardware_concurrency();
        bool enable_console = true;
        std::string logfile;
        std::string bootstrap;
        std::string loglevel;

        int argn = 1;
        if (argc <= argn)
        {
            usage();
            return -1;
        }
        bootstrap = argv[argn++];

        if (fs::path(bootstrap).extension() != ".lua")
        {
            usage();
            return -1;
        }

        std::string arg = "return {";
        for (int i = argn; i < argc; ++i)
        {
            arg.append("'");
            arg.append(argv[i]);
            arg.append("',");
        }
        arg.append("}");

        if(file::read_all(bootstrap, std::ios::in).substr(0, 11) == "---__init__")
        {
            std::unique_ptr<lua_State, moon::state_deleter> lua_{ luaL_newstate() };
            lua_State* L = lua_.get();
            luaL_openlibs(L);
            lua_pushboolean(L, true);
            lua_setglobal(L, "__init__");

            lua_pushcfunction(L, traceback);
            assert(lua_gettop(L) == 1);

            int r = luaL_loadfile(L, bootstrap.data());
            MOON_CHECK(r == LUA_OK, moon::format("loadfile %s", lua_tostring(L, -1)));

            r = luaL_dostring(L, arg.data());
            MOON_CHECK(r == LUA_OK, moon::format("%s", lua_tostring(L, -1)));
            r = lua_pcall(L, 1, 1, 1);
            MOON_CHECK(r == LUA_OK, moon::format("%s", lua_tostring(L, -1)));
            MOON_CHECK(lua_type(L, -1) == LUA_TTABLE, "init must return conf table");
            lua_pushnil(L);
            while (lua_next(L, -2))
            {
                std::string key = lua_tostring(L, -2);
                if (key == "thread")
                    thread_count = (uint32_t)luaL_checkinteger(L, -1);
                else if (key == "logfile")
                    logfile = luaL_check_stringview(L, -1);
                else if (key == "enable_console")
                    enable_console = lua_toboolean(L, -1);
                else if (key == "loglevel")
                    loglevel = luaL_check_stringview(L, -1);
                lua_pop(L, 1);
            }
        }

        std::shared_ptr<server> server_ = std::make_shared<server>();
        wk_server = server_;

        server_->register_service("lua", []()->service_ptr_t {
            return std::make_unique<lua_service>();
            });

#if TARGET_PLATFORM == PLATFORM_WINDOWS
        server_->set_env("LUA_CPATH_EXT", "/?.dll;");
#else
        server_->set_env("LUA_CPATH_EXT", "/?.so;");
#endif

        {
            auto p = fs::current_path();
            p /= "lualib";
            if (!fs::exists(p))
            {
                p = directory::module_path();
                p /= "lualib";
            }
            std::string lualibdir = p.string();
            moon::replace(lualibdir, "\\", "/");
            server_->set_env("PATH", moon::format("package.path='%s/?.lua;'..package.path", lualibdir.data()));
        }

        fs::current_path(fs::absolute(fs::path(bootstrap)).parent_path());
        directory::working_directory = fs::current_path();

        server_->set_env("ARG", arg);
        server_->set_env("THREAD_NUM", std::to_string(thread_count));

        server_->logger()->set_enable_console(enable_console);
        server_->logger()->set_level(loglevel);

        server_->init(thread_count, logfile);

        service_conf conf;
        conf.name = "bootstrap";
        conf.source = fs::path(bootstrap).filename().string();
        server_->new_service("lua", conf, 0, 0);
        server_->set_unique_service("bootstrap", BOOTSTRAP_ADDR);

        server_->run();
    }
    catch (std::exception& e)
    {
        printf("ERROR:%s\n", e.what());
    }

    print_mem_stats();
    return 0;
}

#define REGISTER_CUSTOM_LIBRARY(name, lua_c_fn)\
            int lua_c_fn(lua_State*);\
            luaL_requiref(L, name, lua_c_fn, 0);\
            lua_pop(L, 1);  /* remove lib */\

extern "C"
{
    void open_custom_libs(lua_State* L)
    {
        //core
#ifdef LUA_CACHELIB
        REGISTER_CUSTOM_LIBRARY("codecache", luaopen_cache);
#endif

        REGISTER_CUSTOM_LIBRARY("mooncore", luaopen_moon);
        REGISTER_CUSTOM_LIBRARY("asio", luaopen_asio);
        REGISTER_CUSTOM_LIBRARY("fs", luaopen_fs);
        REGISTER_CUSTOM_LIBRARY("http", luaopen_http);
        REGISTER_CUSTOM_LIBRARY("seri", luaopen_serialize);
        REGISTER_CUSTOM_LIBRARY("json", luaopen_json);
        REGISTER_CUSTOM_LIBRARY("buffer", luaopen_buffer);
        REGISTER_CUSTOM_LIBRARY("sharetable.core", luaopen_sharetable_core);
        REGISTER_CUSTOM_LIBRARY("socket.core", luaopen_socket_core);

        //custom
        REGISTER_CUSTOM_LIBRARY("pb", luaopen_pb);
        REGISTER_CUSTOM_LIBRARY("pb.unsafe", luaopen_pb_unsafe);
        REGISTER_CUSTOM_LIBRARY("crypt", luaopen_crypt);
        REGISTER_CUSTOM_LIBRARY("aoi", luaopen_aoi);
        REGISTER_CUSTOM_LIBRARY("clonefunc", luaopen_clonefunc);
        REGISTER_CUSTOM_LIBRARY("random", luaopen_random);
        REGISTER_CUSTOM_LIBRARY("zset", luaopen_zset);
    }
}

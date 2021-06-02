#include "lua_service.h"
#include "message.hpp"
#include "server.h"
#include "worker.h"
#include "common/hash.hpp"
#include "common/lua_utility.hpp"

#ifdef MOON_ENABLE_MIMALLOC
#include "mimalloc.h"
#define free mi_free 
#define realloc mi_realloc 
#endif

using namespace moon;

constexpr size_t mb_memory = 1024 * 1024;

extern "C" void open_custom_libs(lua_State* L);

void *lua_service::lalloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    lua_service *l = reinterpret_cast<lua_service *>(ud);
    size_t mem = l->mem;

    l->mem += nsize;

    if (ptr)
        l->mem -= osize;

    if (l->mem_limit != 0 && l->mem > l->mem_limit)
    {
        if (ptr == nullptr || nsize > osize)
        {
            l->mem = mem;
            l->logger()->logstring(true, moon::LogLevel::Error,
                                   moon::format("%s Memory error current %.2f M, limit %.2f M", l->name().data(), (float)(l->mem) / mb_memory, (float)l->mem_limit / mb_memory), l->id());
            return nullptr;
        }
    }

    if (l->mem > l->mem_report)
    {
        l->mem_report *= 2;
        l->logger()->logstring(true, moon::LogLevel::Warn,
                               moon::format("%s Memory warning %.2f M", l->name().data(), (float)l->mem / mb_memory), l->id());
    }

    if (nsize == 0)
    {
        free(ptr);
        return nullptr;
    }
    else
    {
        return realloc(ptr, nsize);
    }
}

lua_service::lua_service()
    : lua_(lua_newstate(lalloc, this))
{

}

lua_service::~lua_service()
{
    logger()->logstring(true, moon::LogLevel::Info, moon::format("[WORKER %u] destroy service [%s] ", worker_->id(), name().data()), id());
}

bool lua_service::init(const moon::service_conf& conf)
{
    try
    {
        mem_limit = conf.memlimit;
        name_ = conf.name;

        lua_State* L = lua_.get();
        lua_gc(L, LUA_GCSTOP, 0);
        lua_gc(L, LUA_GCGEN, 0, 0);

        luaL_openlibs(L);

        lua_pushlightuserdata(L, this);
        lua_setfield(L, LUA_REGISTRYINDEX, LMOON_GLOBAL);

        open_custom_libs(L);

        int r = luaL_dostring(L, server_->get_env("PATH").data());
        MOON_CHECK(r == LUA_OK, moon::format("PATH %s", lua_tostring(L, -1)));

        lua_pushcfunction(L, traceback);
        assert(lua_gettop(L) == 1);

        r = luaL_loadfile(L, conf.source.data());
        MOON_CHECK(r == LUA_OK, moon::format("loadfile %s", lua_tostring(L, -1)));

        int nargs = (conf.params.empty()?0:1);
        if (nargs > 0)
        {
            r = luaL_dostring(L, conf.params.data());
            MOON_CHECK(r == LUA_OK, moon::format("params %s", lua_tostring(L, -1)));
        }

        r = lua_pcall(L, nargs, 0, 1);
        MOON_CHECK(r == LUA_OK, moon::format("run %s", lua_tostring(L, -1)));

        lua_settop(L, 0);

        if (unique())
        {
            MOON_CHECK(server_->set_unique_service(name(), id()), moon::format("lua service init failed: unique service name %s repeated.", name().data()).data());
        }

        logger()->logstring(true, moon::LogLevel::Info, moon::format("[WORKER %u] new service [%s]", worker_->id(), name().data()), id());
        ok_ = true;
        lua_gc(L, LUA_GCRESTART, 0);
    }
    catch (const std::exception &e)
    {
        CONSOLE_ERROR(logger(), "lua_service::init error:\n%s.", e.what());
    }
    return ok_;
}

void lua_service::dispatch(message *msg)
{
    if (!ok())
        return;
    lua_State* L = lua_.get();
    try
    {
        int trace = 1;
        int top = lua_gettop(L);
        if (top == 0)
        {
            lua_pushcfunction(L, traceback);
            lua_rawgetp(L, LUA_REGISTRYINDEX, this);
        }
        else
        {
            assert(top == 2);
        }
        lua_pushvalue(L, 2);

        lua_pushlightuserdata(L, msg);
        lua_pushinteger(L, msg->type());

        int r = lua_pcall(L, 2, 0, trace);
        if (r == LUA_OK) {
            return;
        }

        std::string error;

        switch (r) {
        case LUA_ERRRUN:
            error = moon::format("dispatch %s error:\n%s", name().data(), lua_tostring(L, -1));
            break;
        case LUA_ERRMEM:
            error = moon::format("dispatch %s memory error", name().data());
            break;
        case LUA_ERRERR:
            error = moon::format("dispatch %s error in error", name().data());
            break;
        };

        lua_pop(L, 1);

        if (msg->sessionid() >= 0)
        {
            logger()->logstring(true, moon::LogLevel::Error, error, id());
        }
        else
        {
            msg->set_sessionid(-msg->sessionid());
            server_->response(msg->sender(), "dispatch "sv, error, msg->sessionid(), PTYPE_ERROR);
        }
    }
    catch (const std::exception& e)
    {
        luaL_traceback(L, L, e.what(), 1);
        const char* trace = lua_tostring(L, -1);
        if (nullptr == trace)
        {
            trace = "";
        }
        CONSOLE_ERROR(logger(), "dispatch:\n%s", trace);
        lua_pop(L, 1);
    }
}


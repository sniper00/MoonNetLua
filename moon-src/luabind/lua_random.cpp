
#include "lua.hpp"
#include <cstdint>
#include <numeric>
#include <iterator>
#include "common/random.hpp"
#include "common/lua_utility.hpp"
#include "common/utils.hpp"

//[min,max]
static int lrand_range(lua_State *L)
{
    int64_t v_min = (int64_t)luaL_checkinteger(L, 1);
    int64_t v_max = (int64_t)luaL_checkinteger(L, 2);
    if (v_min > v_max)
    {
        return luaL_error(L, "argument error: #1 > #2");
    }
    auto res = moon::rand_range(v_min, v_max);
    lua_pushinteger(L, res);
    return 1;
}

//[min,max]
static int lrand_range_some(lua_State *L)
{
    int64_t v_min = (int64_t)luaL_checkinteger(L, 1);
    int64_t v_max = (int64_t)luaL_checkinteger(L, 2);
    int64_t v_count = (int64_t)luaL_checkinteger(L, 3);

    if (v_count <= 0 || (v_max - v_min + 1) < v_count)
    {
        return luaL_error(L, "rand_range_some range count < num");
    }

    std::vector<int64_t> vec(v_max - v_min+1);
    std::iota(vec.begin(), vec.end(), v_min);

    lua_createtable(L, (int)v_count, 0);
    int count = 0;
    while (v_count > 0)
    {
        auto index = moon::rand_range((size_t)0, vec.size() - 1);
        lua_pushinteger(L, vec[index]);
        lua_rawseti(L, -2, ++count);
        vec[index] = vec[vec.size() - 1];
        vec.pop_back();
        --v_count;
    }
    return 1;
}

//[min,max)
static int lrandf_range(lua_State *L)
{
    double v_min = (double)luaL_checknumber(L, 1);
    double v_max = (double)luaL_checknumber(L, 2);
    auto res = moon::randf_range(v_min, v_max);
    lua_pushnumber(L, res);
    return 1;
}

static int lrandf_percent(lua_State *L)
{
    double v = (double)luaL_checknumber(L, 1);
    auto res = moon::randf_percent(v);
    lua_pushboolean(L, res);
    return 1;
}

static int lrand_weight(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TTABLE);

    moon::lua_array_view<int64_t> values{ L,1 };
    moon::lua_array_view<int64_t> weights{ L,2 };

    if (values.empty() || weights.empty() || values.size() != weights.size())
    {
        return luaL_error(L, "lrand_weight values size != weights size");
    }

    int64_t sum = std::accumulate(weights.begin(), weights.end(), int64_t{ 0 });
    if (sum == 0)
    {
        return 0;
    }

    int64_t cutoff = moon::rand_range(int64_t{ 0 }, sum - 1);
    auto vi = values.begin();
    auto wi = weights.begin();
    while (cutoff >= * wi)
    {
        cutoff -= *wi++;
        ++vi;
    }
    lua_pushinteger(L, *vi);
    return 1;
}

static int lrand_weight_some(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TTABLE);
    int64_t count = luaL_checkinteger(L, 3);

    moon::lua_array_view<int64_t> values{ L,1 };
    moon::lua_array_view<int64_t> weights{ L,2 };

    if (values.size() != weights.size())
    {
        return luaL_error(L, "lrand_weight_some values size != weights size");
    }

    if (values.empty() || weights.empty() || count <= 0 || values.size() < (size_t)count)
    {
        return 0;
    }

    std::vector<int64_t> v;
    std::vector<int64_t> w;

    std::copy(values.begin(), values.end(), std::back_inserter(v));
    std::copy(weights.begin(), weights.end(), std::back_inserter(w));

    lua_createtable(L, (int)count, 0);
    for (int64_t i = 0; i < count; ++i)
    {
        int64_t sum = std::accumulate(w.begin(), w.end(), int64_t{ 0 });
        if (sum == 0)
        {
            lua_pop(L, 1);//pop table
            return 0;
        }
        int64_t cutoff = moon::rand_range(int64_t{ 0 }, sum - 1);
        auto idx = 0;
        while (cutoff >= w[idx])
        {
            cutoff -= w[idx];
            ++idx;
        }
        lua_pushinteger(L, v[idx]);
        lua_rawseti(L, -2, i + 1);
        v[idx] = v[v.size() - 1];
        v.pop_back();
        w[idx] = w[w.size() - 1];
        w.pop_back();
    }
    return 1;
}

extern "C"
{
    int LUAMOD_API luaopen_random(lua_State *L)
    {
        luaL_Reg l[] = {
            {"rand_range",lrand_range},
            {"rand_range_some",lrand_range_some},
            {"randf_range",lrandf_range },
            {"randf_percent",lrandf_percent },
            {"rand_weight",lrand_weight },
            {"rand_weight_some",lrand_weight_some },
            {NULL,NULL}
        };
        luaL_checkversion(L);
        luaL_newlib(L, l);
        return 1;
    }
}

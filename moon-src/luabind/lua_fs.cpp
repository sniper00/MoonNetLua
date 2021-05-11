#include "lua.hpp"
#include "common/directory.hpp"
#include "common/lua_utility.hpp"

using namespace moon;

static int lfs_listdir(lua_State* L)
{
    std::string_view dir = luaL_check_stringview(L, 1);
    int depth = (int)luaL_optinteger(L, 2, 0);
    const char* ext = luaL_optstring(L, 3, nullptr);
    int idx = 0;
    lua_createtable(L, 32, 0);
    directory::scan_dir(dir, depth, [L, &idx, ext](const fs::path& path, bool isdir)->bool {
        if (nullptr != ext)
        {
            if (!isdir && path.extension().string() == ext)
            {
                std::string v = path.string();
                lua_pushlstring(L, v.data(), v.size());
                lua_rawseti(L, -2, ++idx);
            }
        }
        else
        {
            std::string v = path.string();
            lua_pushlstring(L, v.data(), v.size());
            lua_rawseti(L, -2, ++idx);
        }
        return true;
        });
    return 1;
}

static int lfs_isdir(lua_State* L)
{
    try
    {
        std::string_view dir = luaL_check_stringview(L, 1);
        bool res = fs::is_directory(dir);
        lua_pushboolean(L, res ? 1 : 0);
        return 1;
    }
    catch (std::exception& e)
    {
        return luaL_error(L, "fs.isdir %s", e.what());
    }
}

static int lfs_join(lua_State* L)
{
    try
    {
        fs::path dir1 = fs::path{ luaL_check_stringview(L, 1) };
        fs::path dir2 = fs::path{ luaL_check_stringview(L, 2) };
        dir1 /= dir2;
        std::string s = dir1.string();
        lua_pushlstring(L, s.data(), s.size());
        return 1;
    }
    catch (std::exception& e)
    {
        return luaL_error(L, "fs.join %s", e.what());
    }
}

static int lfs_exists(lua_State* L)
{
    try
    {
        bool res = fs::exists(luaL_check_stringview(L, 1));
        lua_pushboolean(L, res ? 1 : 0);
        return 1;
    }
    catch (std::exception& e)
    {
        return luaL_error(L, "fs.exists %s", e.what());
    }
}

static int lfs_mkdir(lua_State* L)
{
    try
    {
        bool res = fs::create_directories(luaL_check_stringview(L, 1));
        lua_pushboolean(L, res ? 1 : 0);
        return 1;
    }
    catch (std::exception& e)
    {
        return luaL_error(L, "fs.mkdir %s", e.what());
    }
}

static int lfs_remove(lua_State* L)
{
    try
    {
        bool res = false;
        if (lua_isnoneornil(L, 2))
            res = fs::remove(luaL_check_stringview(L, 1));
        else
            res = fs::remove_all(luaL_check_stringview(L, 1));
        lua_pushboolean(L, res ? 1 : 0);
        return 1;
    }
    catch (std::exception& e)
    {
        return luaL_error(L, "fs.remove %s", e.what());
    }
}

static int lfs_cwd(lua_State* L)
{
    try
    {
        std::string s = directory::working_directory.string();
        lua_pushlstring(L, s.data(), s.size());
        return 1;
    }
    catch (std::exception& e)
    {
        return luaL_error(L, "fs.cwd %s", e.what());
    }
}

static int lfs_split(lua_State* L)
{
    try
    {
        fs::path dir = fs::absolute(luaL_check_stringview(L, 1));
        std::string dirname = dir.parent_path().string();
        lua_pushlstring(L, dirname.data(), dirname.size());
        std::string basename = dir.filename().string();
        lua_pushlstring(L, basename.data(), basename.size());
        return 2;
    }
    catch (std::exception& e)
    {
        return luaL_error(L, "fs.split %s", e.what());
    }
}

static int lfs_ext(lua_State* L)
{
    try
    {
        fs::path dir = fs::absolute(luaL_check_stringview(L, 1));
        std::string ext = dir.extension().string();
        lua_pushlstring(L, ext.data(), ext.size());
        return 1;
    }
    catch (std::exception& e)
    {
        return luaL_error(L, "fs.ext %s", e.what());
    }
}

static int lfs_root(lua_State* L)
{
    try
    {
        fs::path dir = fs::absolute(luaL_check_stringview(L, 1));
        std::string s = dir.root_path().string();
        lua_pushlstring(L, s.data(), s.size());
        return 1;
    }
    catch (std::exception& e)
    {
        return luaL_error(L, "fs.root %s", e.what());
    }
}

static int lfs_stem(lua_State* L)
{
    try
    {
        fs::path dir = fs::path(luaL_check_stringview(L, 1));
        std::string s = dir.stem().string();
        lua_pushlstring(L, s.data(), s.size());
        return 1;
    }
    catch (std::exception& e)
    {
        return luaL_error(L, "fs.stem %s", e.what());
    }
}

static int lfs_abspath(lua_State* L)
{
    try
    {
        fs::path dir = fs::absolute(luaL_check_stringview(L, 1));
        std::string s = dir.string();
        lua_pushlstring(L, s.data(), s.size());
        return 1;
    }
    catch (std::exception& e)
    {
        return luaL_error(L, "fs.abspath %s", e.what());
    }
}

extern "C"
{
    int LUAMOD_API luaopen_fs(lua_State* L)
    {
        luaL_Reg l[] = {
            { "listdir", lfs_listdir},
            { "isdir", lfs_isdir },
            { "join", lfs_join },
            { "exists", lfs_exists },
            { "mkdir", lfs_mkdir},
            { "remove", lfs_remove},
            { "cwd", lfs_cwd},
            { "split", lfs_split},
            { "ext", lfs_ext},
            { "root", lfs_root},
            { "stem", lfs_stem},
            { "abspath", lfs_abspath},
            {NULL,NULL}
        };
        luaL_newlib(L, l);
        return 1;
    }
}

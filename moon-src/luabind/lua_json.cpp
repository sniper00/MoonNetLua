#include "lua.hpp"
#include <vector>
#include <charconv>
#include <string_view>

// __SSE2__ and __SSE4_2__ are recognized by gcc, clang, and the Intel compiler.
// We use -march=native with gmake to enable -msse2 and -msse4.2, if supported.
#if defined(__SSE4_2__)
#  define RAPIDJSON_SSE42
#elif defined(__SSE2__)
#  define RAPIDJSON_SSE2
#endif

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/reader.h"
#include "rapidjson/error/en.h"

#include "common/buffer.hpp"

#ifdef MOON_ENABLE_MIMALLOC
#include "mimalloc.h"
class MimallocAllocator {
public:
    static const bool kNeedFree = true;
    void* Malloc(size_t size) {
        if (size) //  behavior of malloc(0) is implementation defined.
            return mi_malloc(size);
        else
            return NULL; // standardize to returning NULL.
    }
    void* Realloc(void* originalPtr, size_t originalSize, size_t newSize) {
        (void)originalSize;
        if (newSize == 0) {
            mi_free(originalPtr);
            return NULL;
        }
        return mi_realloc(originalPtr, newSize);
    }
    static void Free(void* ptr) { mi_free(ptr); }
};
    using JsonAllocator = MimallocAllocator;
#else
    using JsonAllocator = rapidjson::CrtAllocator;
#endif

using StreamBuf = rapidjson::GenericStringBuffer<rapidjson::UTF8<>, JsonAllocator>;
using JsonWriter = rapidjson::Writer<StreamBuf>;
using JsonPrettyWriter = rapidjson::PrettyWriter<StreamBuf>;

using JsonReader = rapidjson::GenericReader<rapidjson::UTF8<>, rapidjson::UTF8<>, JsonAllocator>;

static constexpr int max_depth = 32;

namespace rapidjson
{
    // StringNStream
    //! Read-only string stream.
    /*! \note implements Stream concept
    */
    template <typename Encoding>
    struct GenericStringNStream {
        typedef typename Encoding::Ch Ch;

        GenericStringNStream(const Ch* src, const size_t len) : src_(src), head_(src), len_(len) {}

        Ch Peek() const { return Tell() < len_ ? *src_ : '\0'; }
        Ch Take() { return *src_++; }
        size_t Tell() const { return static_cast<size_t>(src_ - head_); }

        Ch* PutBegin() { RAPIDJSON_ASSERT(false); return 0; }
        void Put(Ch) { RAPIDJSON_ASSERT(false); }
        void Flush() { RAPIDJSON_ASSERT(false); }
        size_t PutEnd(Ch*) { RAPIDJSON_ASSERT(false); return 0; }

        const Ch* src_;     //!< Current read position.
        const Ch* head_;    //!< Original head of the string.
        size_t len_;    //!< Original head of the string.
    };
    //! String stream with UTF8 encoding.
    typedef GenericStringNStream<UTF8<> > StringNStream;

    template <typename Encoding>
    struct StreamTraits<GenericStringNStream<Encoding> > {
        enum { copyOptimization = 1 };
    };
}

using StringNStream = rapidjson::StringNStream;

template<typename Writer>
static void encode_table(lua_State* L, Writer* writer, int idx, int depth, bool empty_as_array);

template<typename Writer>
static void encode_one(lua_State* L, Writer* writer, int idx, int depth = 0, bool empty_as_array = true)
{
    int t = lua_type(L, idx);
    switch (t)
    {
    case LUA_TBOOLEAN:
    {
        writer->Bool(lua_toboolean(L, idx) != 0);
        return;
    }
    case LUA_TNUMBER:
    {
        if (lua_isinteger(L, idx))
        {
            writer->Int64(lua_tointeger(L, idx));
        }
        else
        {
            if (!writer->Double(lua_tonumber(L, idx)))
                luaL_error(L, "error while encode double value.");
        }
        return;
    }
    case LUA_TSTRING:
    {
        size_t len = 0;
        const char* str = lua_tolstring(L, idx, &len);
        writer->String(str, static_cast<rapidjson::SizeType>(len));
        return;
    }
    case LUA_TTABLE:
    {
        encode_table(L, writer, idx, depth + 1, empty_as_array);
        return;
    }
    case LUA_TNIL:
    {
        writer->Null();
        return;
    }
    default:
        luaL_error(L, "json encode: unsupport value type : %s", lua_typename(L, t));
    }
}

static inline size_t array_size(lua_State* L, int index)
{
    int arr_size = (int)lua_rawlen(L, index);
    if (arr_size == 0)
    {
        return arr_size;
    }
    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        if (lua_type(L, -2) == LUA_TNUMBER) 
        {
            if (lua_isinteger(L, -2)) 
            {
                lua_Integer x = lua_tointeger(L, -2);
                if (x > 0 && x <= arr_size) 
                {
                    lua_pop(L, 1);
                    continue;
                }
            }
        }
        lua_pop(L, 2);
        return 0;
    }
    return arr_size;
}

template<typename Writer>
static void encode_table(lua_State* L, Writer* writer, int idx, int depth, bool empty_as_array)
{
    if (depth > max_depth)
        luaL_error(L, "nested too depth");

    if (idx < 0) {
        idx = lua_gettop(L) + idx + 1;
    }

    luaL_checkstack(L, LUA_MINSTACK, NULL);

    if (size_t size = array_size(L, idx); size >0)
    {
        writer->StartArray();
        for (size_t i = 1; i <= size; i++)
        {
            lua_rawgeti(L, idx, i);
            encode_one(L, writer, -1, depth, empty_as_array);
            lua_pop(L, 1);
        }
        writer->EndArray();
    }
    else
    {
        bool empty = true;
        lua_pushnil(L);
        while (lua_next(L, idx))
        {
            if (empty)
            {
                writer->StartObject();
                empty = false;
            }

            int key_type = lua_type(L, -2);
            switch (key_type)
            {
            case LUA_TSTRING:
            {
                size_t len = 0;
                const char* key = lua_tolstring(L, -2, &len);
                writer->Key(key, static_cast<rapidjson::SizeType>(len));
                encode_one(L, writer, -1, depth, empty_as_array);
                break;
            }
            case LUA_TNUMBER:
            {
                lua_Integer key = luaL_checkinteger(L, -2);
                char tmp[256];
                auto res = std::to_chars(tmp, tmp + 255, key);
                if (res.ec != std::errc())
                {
                    luaL_error(L, "json encode: int key to_chars failed.");
                }
                writer->Key(tmp, static_cast<rapidjson::SizeType>(res.ptr - tmp), true);
                encode_one(L, writer, -1, depth, empty_as_array);
                break;
            }
            default:
                luaL_error(L, "json encode: object only support string and integer key");
                break;
            }
            lua_pop(L, 1);
        }

        if (!empty)
        {
            writer->EndObject();
        }
        else
        {
            if (empty_as_array)
            {
                writer->StartArray();
                writer->EndArray();
            }
            else
            {
                writer->StartObject();
                writer->EndObject();
            }
        }
    }
}

static int  do_encode(lua_State* L, int index, bool empty_as_array, moon::buffer* buf)
{
    if (index < 0) {
        index = lua_gettop(L) + index + 1;
    }

    StreamBuf stream;
    JsonWriter writer(stream);
    encode_one(L, &writer, index, 0, empty_as_array);
    if (nullptr != buf)
    {
        buf->write_back(stream.GetString(), stream.GetSize());
        return 0;
    }
    else
    {
        lua_pushlstring(L, stream.GetString(), stream.GetSize());
        return 1;
    }
}

static int  encode(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    bool empty_as_array = (bool)luaL_opt(L, lua_toboolean, 2, true);
    lua_settop(L, 1);
    return do_encode(L, 1, empty_as_array, nullptr);
}

static int  pretty_encode(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    bool empty_as_array = (bool)luaL_opt(L, lua_toboolean, 2, true);
    lua_settop(L, 1);
    StreamBuf stream;
    JsonPrettyWriter writer(stream);
    encode_one(L, &writer, 1, 0, empty_as_array);
    lua_pushlstring(L, stream.GetString(), stream.GetSize());
    return 1;
}

static int  concat(lua_State* L)
{
    if (lua_type(L, 1) == LUA_TSTRING)
    {
        size_t size;
        const char* sz = lua_tolstring(L, -1, &size);
        auto buf = new moon::buffer(256, 16);
        buf->write_back(sz, size);
        lua_pushlightuserdata(L, buf);
        return 1;
    }

    luaL_checktype(L, 1, LUA_TTABLE);
    auto buf = new moon::buffer(256, 16);
    int array_size = (int)lua_rawlen(L, 1);
    for (int i = 1; i <= array_size; i++) {
        lua_rawgeti(L, 1, i);
        switch (lua_type(L, -1))
        {
        case LUA_TNUMBER:
        {
            if (lua_isinteger(L, -1))
            {
                buf->write_chars(lua_tointeger(L, -1));
            }
            else
            {
                std::string s = std::to_string(lua_tonumber(L, -1));
                buf->write_back(s.data(), s.size());
            }
            break;
        }
        case LUA_TBOOLEAN:
        {
            const char* v = lua_toboolean(L, -1) ? "true" : "false";
            buf->write_back(v, strlen(v));
            break;
        }
        case LUA_TSTRING:
        {
            size_t size;
            const char* sz = lua_tolstring(L, -1, &size);
            buf->write_back(sz, size);
            break;
        }
        case LUA_TTABLE:
        {
            do_encode(L, -1, true, buf);
            break;
        }
        default:
            break;
        }
        lua_pop(L, 1);
    }
    lua_pushlightuserdata(L, buf);
    return 1;
}

static void write_resp(moon::buffer* buf, const char* cmd, size_t size)
{
    buf->write_back("\r\n$", 3);
    buf->write_chars(size);
    buf->write_back("\r\n", 2);
    buf->write_back(cmd, size);
}

static int concat_resp(lua_State* L) {
    int n = lua_gettop(L);
    if (0 == n)
    {
        return 0;
    }

    auto buf = new moon::buffer(256, 16);
    buf->write_back("*", 1);
    buf->write_chars(n);

    for (int i = 1; i <= n; i++) {
        switch (lua_type(L, i))
        {
        case LUA_TNIL:
        {
            std::string_view sv = "\r\n$-1";
            buf->write_back(sv.data(), sv.size());
            break;
        }
        case LUA_TNUMBER:
        {
            if (lua_isinteger(L, i))
            {
                std::string s = std::to_string(lua_tointeger(L, i));
                write_resp(buf, s.data(), s.size());
            }
            else
            {
                std::string s = std::to_string(lua_tonumber(L, i));
                write_resp(buf, s.data(), s.size());
            }
            break;
        }
        case LUA_TBOOLEAN:
        {
            std::string_view sv = lua_toboolean(L, i) ? "true" : "false";
            write_resp(buf, sv.data(), sv.size());
            break;
        }
        case LUA_TSTRING:
        {
            size_t msize;
            const char* sz = lua_tolstring(L, i, &msize);
            write_resp(buf, sz, msize);
            break;
        }
        case LUA_TTABLE:
        {
            StreamBuf stream;
            JsonWriter writer(stream);
            encode_one(L, &writer, i, 0, true);
            write_resp(buf, stream.GetString(), stream.GetSize());
            break;
        }
        default:
            break;
        }
    }
    buf->write_back("\r\n", 2);
    lua_pushlightuserdata(L, buf);
    return 1;
}

struct LuaPushHandler {
    explicit LuaPushHandler(lua_State* aL) : L(aL) {}

    bool Null() {
        static int json_null = LUA_NOREF;
        lua_rawgeti(L, LUA_REGISTRYINDEX, json_null);
        Commit();
        return true;
    }

    bool Bool(bool b) {
        lua_pushboolean(L, b);
        Commit();
        return true;
    }

    bool Int(int i) {
        lua_pushinteger(L, i);
        Commit();
        return true;
    }

    bool Uint(unsigned u) {
        lua_pushinteger(L, u);
        Commit();
        return true;
    }

    bool Int64(int64_t i) {
        lua_pushinteger(L, i);
        Commit();
        return true;
    }

    bool Uint64(uint64_t u) {
        lua_pushinteger(L, u);
        Commit();
        return true;
    }

    bool Double(double d) {
        lua_pushnumber(L, static_cast<lua_Number>(d));
        Commit();
        return true;
    }

    bool RawNumber(const char* str, rapidjson::SizeType length, bool /*copy*/) {
        lua_getglobal(L, "tonumber");
        lua_pushlstring(L, str, length);
        lua_call(L, 1, 1);
        Commit();
        return true;
    }

    bool String(const char* str, rapidjson::SizeType length, bool /*copy*/) {
        lua_pushlstring(L, str, length);
        Commit();
        return true;
    }

    bool StartObject() {
        luaL_checkstack(L, LUA_MINSTACK, NULL);
        lua_createtable(L, 0, 8);
        stack_.push_back(0);// 0 means hash, >0 means array
        return true;
    }

    bool Key(const char* str, rapidjson::SizeType length, bool /*copy*/) const {
        if (length == 0)
        {
            return false;
        }
        char c = str[0];
        if (c == '-' || (c >= '0' && c <= '9'))
        {
            int64_t v = 0;
            auto [p, ec] = std::from_chars(str, str + length, v);
            if (ec != std::errc())
            {
                return false;
            }
            lua_pushinteger(L, v);
        }
        else
        {
            lua_pushlstring(L, str, length);
        }
        return true;
    }

    bool EndObject(rapidjson::SizeType /*memberCount*/) {
        stack_.pop_back();
        Commit();
        return true;
    }

    bool StartArray() {
        luaL_checkstack(L, LUA_MINSTACK, NULL);
        lua_createtable(L, 8, 0);
        stack_.push_back(1);
        return true;
    }

    bool EndArray(rapidjson::SizeType /*elementCount*/) {
        stack_.pop_back();
        Commit();
        return true;
    }

private:
    void Commit()
    {
        if (!stack_.empty())
        {
            //obj
            if (stack_.back() == 0)
            {
                lua_rawset(L, -3);
            }
            else
            {
                lua_rawseti(L, -2, stack_.back()++);
            }
        }
    }
    lua_State* L;
    std::vector<int> stack_;
};

static int lua_json_decode(lua_State* L, const char* s, size_t len)
{
    JsonReader reader;
    StringNStream stream{ s, len };
    LuaPushHandler handler{ L };
    auto res = reader.Parse(stream, handler);
    if (!res) {
        lua_pushnil(L);
        lua_pushfstring(L, "%s (%d)", rapidjson::GetParseError_En(res.Code()), (int)res.Offset());
        return 2;
    }
    return 1;
}

static int decode(lua_State* L)
{
    size_t len = 0;
    const char* str = nullptr;
    if (lua_type(L, 1) == LUA_TSTRING) {
        str = luaL_checklstring(L, 1, &len);
    }
    else {
        str = reinterpret_cast<const char*>(lua_touserdata(L, 1));
        len = luaL_checkinteger(L, 2);
    }

    if (nullptr == str)
    {
        return 0;
    }

    lua_settop(L, 1);

    return lua_json_decode(L, str, len);
}

extern "C"
{
    int LUAMOD_API luaopen_json(lua_State* L)
    {
        luaL_Reg l[] = {
            {"encode", encode},
            {"pretty_encode", pretty_encode},
            {"concat", concat},
            {"concat_resp", concat_resp},
            {"decode", decode},
            {NULL,NULL}
        };
        luaL_newlib(L, l);
        return 1;
    }
}

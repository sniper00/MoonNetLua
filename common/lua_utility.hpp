#pragma once
#include <string_view>
#include "lua.hpp"

#define luaL_rawsetfield(L, tbindex, kname, valueexp)\
    lua_pushliteral(L, kname);\
    (valueexp);\
    lua_rawset(L, tbindex);\

namespace moon
{
    struct lua_scope_pop
    {
        lua_State* lua;
        lua_scope_pop(lua_State* L)
            :lua(L)
        {
        }

        lua_scope_pop(const lua_scope_pop&) = delete;

        lua_scope_pop& operator=(const lua_scope_pop&) = delete;
       
        ~lua_scope_pop()
        {
            lua_pop(lua, 1);
        }
    };

    inline std::string_view luaL_check_stringview(lua_State* L, int index)
    {
        size_t size;
        const char* sz = luaL_checklstring(L, index, &size);
        return std::string_view{ sz, size };
    }

    inline bool luaL_checkboolean(lua_State* L, int index)
    {
        if (!lua_isboolean(L, index))
        {
            luaL_typeerror(L, index, lua_typename(L, LUA_TBOOLEAN));
        }
        return lua_toboolean(L, index);
    }

    inline bool requiref(lua_State* L, const char* name, lua_CFunction f)
    {
        struct protect_call
        {
            static int require_cmodule(lua_State* L)
            {
                const char* name = luaL_checkstring(L, 1);
                lua_CFunction f = (lua_CFunction)lua_touserdata(L, 2);
                luaL_requiref(L, name, f, 0);
                return 0;
            }
        };

        lua_pushcfunction(L, protect_call::require_cmodule);
        lua_pushstring(L, name);
        lua_pushlightuserdata(L, (void*)f);
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            lua_pop(L, 1);
            return false;
        }
        return true;
    }
  
    template<class LuaArrayView>
    class lua_array_view_iterator
    {
        const LuaArrayView* lav_;
        size_t pos_;
    public:
        using iterator_category = std::random_access_iterator_tag;
        using self_type = lua_array_view_iterator;
        using value_type = typename LuaArrayView::value_type;
        using pointer = value_type*;
        using reference = value_type&;
        using difference_type = std::ptrdiff_t;

        explicit lua_array_view_iterator(const LuaArrayView* lav, size_t pos)
            :lav_(lav)
            , pos_(pos)
        {
        }

        auto operator*() const
        {
            return lav_->get_lua_value(pos_ + 1);
        }

        self_type& operator++()
        {
            assert(pos_ != lav_->size());
            ++pos_;
            return *this;
        }

        self_type operator++(int)
        {
            assert(pos_ != lav_->size());
            auto old = pos_;
            pos_++;
            return self_type{ lav_, old };
        }

        difference_type operator-(const self_type& _Right) const
        {	// return difference of iterators
            return (pos_ - _Right.pos_);
        }

        bool operator!=(const self_type& other) const {
            return pos_ != other.pos_;
        }

        bool operator==(const self_type& other) const {
            return pos_ == other.pos_;
        }
    };

    template<typename ValueType>
    class lua_array_view
    {
        int index_;
        lua_State* L_;
        size_t size_ = 0;


        auto get_lua_value(size_t pos) const
        {
            assert(pos <= size());
            lua_rawgeti(L_, index_, pos);
            lua_scope_pop lsp{ L_ };
            if constexpr (std::is_integral_v<value_type>)
            {
                return static_cast<value_type>(luaL_checkinteger(L_, -1));
            }
            else if constexpr (std::is_floating_point_v<value_type>)
            {
                return static_cast<value_type>(luaL_checknumber(L_, -1));
            }
            else
            {
                size_t len;
                const char* data = luaL_checklstring(L_, -1, &len);
                return std::string{ data, len };
            }
        }
    public:
        using value_type = ValueType;

        using const_iterator = lua_array_view_iterator<lua_array_view>;

        friend const_iterator;

        lua_array_view(lua_State* L, int idx)
            :index_(idx)
            , L_(L)
        {
            size_ = lua_rawlen(L, idx);
        }

        size_t size() const
        {
            return size_;
        }

        bool empty() const
        {
            return size_ == 0;
        }

        lua_State* lua_state() const
        {
            return L_;
        }

        value_type operator[](size_t pos) const
        {
            return get_lua_value(pos + 1);
        }

        const_iterator begin() const
        {
            return const_iterator{ this,  0 };
        }

        const_iterator end() const
        {
            return const_iterator{ this,  size() };
        }
    };

    struct state_deleter {
        void operator()(lua_State* L) const {
            lua_close(L);
        }
    };

    inline int traceback(lua_State* L) {
        const char* msg = lua_tostring(L, 1);
        if (msg)
            luaL_traceback(L, L, msg, 1);
        else {
            lua_pushliteral(L, "(no error message)");
        }
        return 1;
    }
}

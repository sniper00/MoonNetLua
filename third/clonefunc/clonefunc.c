#include <lua.h>
#include <lauxlib.h>

#include <lstate.h>
#include <lobject.h>
#include <lfunc.h>
#include <lgc.h>

static int
lclone(lua_State *L) {
	if (!lua_isfunction(L, 1) || lua_iscfunction(L,1))
		return luaL_error(L, "Need lua function");
	const LClosure *c = lua_topointer(L,1);
	int n = (int)luaL_optinteger(L, 2, 0);
	if (n < 0 || n > c->p->sizep)
		return 0;
	luaL_checkstack(L, 1, NULL);
	Proto *p;
	if (n==0) {
		p = c->p;
	} else {
		p = c->p->p[n-1];
	}

	lua_lock(L);
	LClosure *cl = luaF_newLclosure(L, p->sizeupvalues);
	luaF_initupvals(L, cl);
	cl->p = p;
	setclLvalue2s(L, L->top++, cl);
	lua_unlock(L);

	return 1;
}

static int
lproto(lua_State *L) {
	if (!lua_isfunction(L, 1) || lua_iscfunction(L,1))
		return 0;
	const LClosure *c = lua_topointer(L,1);
	lua_pushlightuserdata(L, c->p);
	lua_pushinteger(L, c->p->sizep);
	return 2;
}

int LUAMOD_API
luaopen_clonefunc(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "clone", lclone },
		{ "proto", lproto },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}

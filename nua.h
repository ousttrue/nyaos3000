#ifdef LUA_ENABLE
extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

extern lua_State *nua_init();
extern lua_State *get_nyaos_object(const char *field=NULL,lua_State *L=NULL);

#endif

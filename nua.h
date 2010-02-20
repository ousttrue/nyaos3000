#ifdef LUA_ENABLE
extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
extern lua_State *nua_init();
}
#endif

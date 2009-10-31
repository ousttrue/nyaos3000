#include "shell.h"

#ifdef LUA_ENABLE

#include <stdio.h>
#include <stdlib.h>
#include "nnhash.h"
#include "getline.h"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#define TRACE(X) (X)

extern lua_State *nua_init();
static lua_State *nua=NULL;
static void nua_shutdown()
{
    lua_close(nua);
}

int nua_get(lua_State *lua)
{
    NnHash **dict=(NnHash**)lua_touserdata(lua,-2);
    const char *key=lua_tostring(lua,-1);

    if( dict == NULL || key == NULL ){
        lua_pushnil(lua);
        return 0;
    }
    lua_pushstring( lua , (*dict)->get( NnString(key) )->repr() );
    return 1;
}

int nua_put(lua_State *lua)
{
    NnHash **dict=(NnHash**)lua_touserdata(lua,-3);
    const char *key=lua_tostring(lua,-2);
    const char *val=lua_tostring(lua,-1);

    if( dict && key ){
        if( val ){
            (*dict)->put( NnString(key) , new NnString(val) );
        }else{
            (*dict)->remove( NnString(key) );
        }
    }
    return 0;
}

int nua_items(lua_State *lua)
{
    NnHash **dict=(NnHash**)lua_touserdata(lua,1);
    if( dict == NULL ){
        return 0;
    }

    lua_newtable(lua);
    for(NnHash::Each ptr(**dict) ; *ptr ; ++ptr ){
        lua_pushstring(lua,ptr->key().chars());
        lua_pushstring(lua,ptr->value()->repr());
        lua_settable(lua,-3);
    }
    return 1;
}

lua_State *nua_init()
{
    if( nua == NULL ){
        extern NnHash aliases;
        extern NnHash functions;
        extern NnHash properties;

        static struct {
            const char *name;
            NnHash *dict;
            int (*index)(lua_State *);
            int (*newindex)(lua_State *);
        } list[]={
            { "functions" , &functions , nua_get , NULL },
            { "alias"     , &aliases   , nua_get , nua_put },
            { "suffix"    , &DosShell::executableSuffix , nua_get , nua_put },
            { "properties", &properties , nua_get } ,
            { NULL , NULL , 0 } ,
        }, *p = list;

        nua = luaL_newstate();
        luaL_openlibs(nua);
        atexit(nua_shutdown);

        lua_newtable(nua); /* nyaos = {} */
        while( p->name != NULL ){
            lua_pushstring(nua,p->name);

            NnHash **u=(NnHash**)lua_newuserdata(nua,sizeof(NnHash *));
            *u = p->dict;

            lua_newtable(nua); /* metatable */
            if( p->index != NULL ){
                lua_pushstring(nua,"__index");
                lua_pushcfunction(nua,p->index);
                lua_settable(nua,-3);
            }
            if( p->newindex != NULL ){
                lua_pushstring(nua,"__newindex");
                lua_pushcfunction(nua,p->newindex);
                lua_settable(nua,-3);
            }
            lua_setmetatable(nua,-2);

            lua_settable(nua,-3);
            p++;
        }
        lua_pushstring(nua,"items");
        lua_pushcfunction(nua,nua_items);
        lua_settable(nua,-3);
        lua_setglobal(nua,"nyaos");
    }
    return nua;
}
#endif /* defined(LUA_ENABLED) */

int cmd_require( NyadosShell &shell , const NnString &argv )
{
#ifdef LUA_ENABLE
    NnString arg1,left;
    argv.splitTo( arg1 , left );
    NyadosShell::dequote( arg1 );

    lua_State *nua = nua_init();

    if( luaL_loadfile( nua , arg1.chars() ) ||
        lua_pcall( nua , 0 , 0 , 0 ) )
    {
        const char *msg = lua_tostring( nua , -1 );
        shell.err() << msg << '\n';
    }
#else
    shell.err() << "require: built-in lua disabled.\n";
#endif /* defined(LUA_ENABLED) */
    return 0;
}

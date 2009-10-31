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

//#define TRACE(X) ((X),fflush(stdout))
#define TRACE(X)

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
        return 0;
    }
    NnObject *result=(*dict)->get( NnString(key) );
    if( result != NULL ){
        lua_pushstring( lua , result->repr() );
        return 1;
    }else{
        return 0;
    }
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

int nua_iter(lua_State *lua)
{
    TRACE(puts("Enter: nua_iter") );
    NnHash::Each **ptr=(NnHash::Each**)lua_touserdata(lua,1);

    if( ptr != NULL && ***ptr != NULL ){
        lua_pushstring(lua,(**ptr)->key().chars());
        lua_pushstring(lua,(**ptr)->value()->repr());
        ++(**ptr);
        TRACE(puts("Leave: nua_iter: next") );
        return 2;
    }else{
        TRACE(puts("Leave: nua_iter: last") );
        return 0;
    }
}

int nua_iter_gc(lua_State *lua)
{
    TRACE(puts("Enter: nua_iter_gc") );
    NnHash::Each **ptr=(NnHash::Each**)lua_touserdata(lua,1);
    if( ptr != NULL && *ptr !=NULL ){
        delete *ptr;
        *ptr = NULL;
    }
    TRACE(puts("Leave: nua_iter_gc"));
    return 0;
}

int nua_iter_factory(lua_State *lua)
{
    TRACE(puts("Enter: nua_iter_factory"));
    NnHash **dict=(NnHash**)lua_touserdata(lua,1);
    if( dict == NULL ){
        TRACE(puts("Leave: nua_iter_factory: dict==NULL"));
        return 0;
    }
    NnHash::Each *ptr=new NnHash::Each(**dict);
    if( ptr == NULL ){
        TRACE(puts("Leave: nua_iter_factory: ptr==NULL"));
        return 0;
    }

    lua_pushcfunction(lua,nua_iter);
    void *state = lua_newuserdata(lua,sizeof(NnHash::Each*));
    memcpy(state,&ptr,sizeof(NnHash::Each*));

    lua_newtable(lua);
    lua_pushstring(lua,"__gc");
    lua_pushcfunction(lua,nua_iter_gc);
    lua_settable(lua,-3);
    lua_setmetatable(lua,-2);

    TRACE(puts("Leave: nua_iter_factory: success") );
    return 2;
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
            int (*call)(lua_State *);
        } list[]={
            { "functions" , &functions , nua_get , NULL  , nua_iter_factory},
            { "alias"     , &aliases   , nua_get , nua_put , nua_iter_factory },
            { "suffix"    , &DosShell::executableSuffix , nua_get , nua_put , nua_iter_factory },
            { "properties", &properties , nua_get , nua_put , nua_iter_factory } ,
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
            if( p->call != NULL ){
                lua_pushstring(nua,"__call");
                lua_pushcfunction(nua,p->call);
                lua_settable(nua,-3);
            }
            lua_setmetatable(nua,-2);

            lua_settable(nua,-3);
            p++;
        }
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

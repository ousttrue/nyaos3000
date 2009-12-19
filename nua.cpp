#include "shell.h"

#ifdef LUA_ENABLE

#include <stdio.h>
#include <stdlib.h>
#include "nnhash.h"
#include "getline.h"
#include "nua.h"

//#define TRACE(X) ((X),fflush(stdout))
#define TRACE(X)

extern int open_luautil( lua_State *L );
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

int nua_history_len(lua_State *lua)
{
    History **history=(History**)lua_touserdata(lua,-2);
    if( history == NULL ){
        return 0;
    }
    lua_pushinteger(lua,(*history)->size());
    return 1;
}

int nua_history_get(lua_State *lua)
{
    History **history=(History**)lua_touserdata(lua,-2);
    int key=lua_tointeger(lua,-1);

    if( history == NULL || !lua_isnumber(lua,-1) ){
        return 0;
    }
    NnObject *result=(**history)[key-1];
    if( result != NULL ){
        lua_pushstring( lua , result->repr() );
        return 1;
    }else{
        return 0;
    }
}


class NnLuaFunction : public NnExecutable {
public:
    int operator()( const NnVector &args );
};

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

int nua_history_iter(lua_State *lua)
{
    History **history=(History**)lua_touserdata(lua,-2);
    if( history == NULL || !lua_isnumber(lua,-1) ){
        return 0;
    }
    int n=lua_tointeger(lua,-1);
    if( n >= (*history)->size() ){
        return 0;
    }
    lua_pushinteger(lua,n+1);
    lua_pushstring(lua,(**history)[n]->repr());

    return 2;
}

int nua_history_iter_factory(lua_State *lua)
{
    History **history=(History**)lua_touserdata(lua,-1);
    if( history == NULL ){
        return 0;
    }
    lua_pushcfunction(lua,nua_history_iter);
    lua_insert(lua,-2);
    lua_pushinteger(lua,0);
    return 3;
}


lua_State *nua_init()
{
    if( nua == NULL ){
        extern NnHash aliases;
        extern NnHash properties;

        static struct {
            const char *name;
            NnHash *dict;
            int (*index)(lua_State *);
            int (*newindex)(lua_State *);
            int (*call)(lua_State *);
        } list[]={
            { "alias"  , &aliases   , nua_get , nua_put , nua_iter_factory },
            { "suffix" , &DosShell::executableSuffix , nua_get , nua_put , nua_iter_factory },
            { "option" , &properties , nua_get , nua_put , nua_iter_factory } ,
            { NULL , NULL , 0 } ,
        }, *p = list;

        nua = luaL_newstate();
        luaL_openlibs(nua);
        atexit(nua_shutdown);

        open_luautil(nua); 
        lua_newtable(nua);
        lua_setfield(nua,-2,"command");

        while( p->name != NULL ){
            NnHash **u=(NnHash**)lua_newuserdata(nua,sizeof(NnHash *));
            *u = p->dict;

            lua_newtable(nua); /* metatable */
            if( p->index != NULL ){
                lua_pushcfunction(nua,p->index);
                lua_setfield(nua,-2,"__index");
            }
            if( p->newindex != NULL ){
                lua_pushcfunction(nua,p->newindex);
                lua_setfield(nua,-2,"__newindex");
            }
            if( p->call != NULL ){
                lua_pushcfunction(nua,p->call);
                lua_setfield(nua,-2,"__call");
            }
            lua_setmetatable(nua,-2);

            lua_setfield(nua,-2,p->name);
            p++;
        }
        History **h=(History**)lua_newuserdata(nua,sizeof(History*));
        *h = &GetLine::history;
        lua_newtable(nua);
        lua_pushcfunction(nua,nua_history_get);
        lua_setfield(nua,-2,"__index");
        lua_pushcfunction(nua,nua_history_len);
        lua_setfield(nua,-2,"__len");
        lua_pushcfunction(nua,nua_history_iter_factory);
        lua_setfield(nua,-2,"__call");
        lua_setmetatable(nua,-2);
        lua_setfield(nua,-2,"history");

        lua_setglobal(nua,"nyaos");
    }
    return nua;
}
#endif /* defined(LUA_ENABLED) */

int cmd_lua_l( NyadosShell &shell , const NnString &argv )
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

int cmd_lua_e( NyadosShell &shell , const NnString &argv )
{
#ifdef LUA_ENABLE
    NnString arg1,left;
    argv.splitTo( arg1 , left );
    NyadosShell::dequote( arg1 );

    if( arg1.empty() ){
        shell.err() << "lua_e \"lua-code\"\n" ;
        return 0;
    }
    lua_State *nua = nua_init();

    if( luaL_loadstring( nua ,  luaL_gsub( nua , arg1.chars() , "$T" , "\n" ) ) ||
        lua_pcall( nua , 0 , 0 , 0 ) )
    {
        const char *msg = lua_tostring( nua , -1 );
        shell.err() << msg << '\n';
    }
    lua_settop(nua,0);
#else
    shell.err() << "require: built-in lua disabled.\n";
#endif /* defined(LUA_ENABLED) */
    return 0;
}

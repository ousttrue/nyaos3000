#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}
#include <sys/types.h>
#include <dirent.h>

const static char NYAOS_OPENDIR[] = "nyaos_opendir";

static int luaone_chdir(lua_State *L)
{
    const char *newdir = lua_tostring(L,1);
    if( newdir != NULL ){
        chdir( newdir );
    }
    return 0;
}

static int luaone_bitand(lua_State *L)
{
    int left=luaL_checkint(L,1);
    int right=luaL_checkint(L,2);
    lua_pushinteger(L,left & right);
    return 1;
}

static int luaone_bitor(lua_State *L)
{
    int left=luaL_checkint(L,1);
    int right=luaL_checkint(L,2);
    lua_pushinteger(L,left | right);
    return 1;
}

static int luaone_bitxor(lua_State *L)
{
    int left=luaL_checkint(L,1);
    int right=luaL_checkint(L,2);
    lua_pushinteger(L,left ^ right);
    return 1;
}

static int luaone_rshift(lua_State *L)
{
    int left=luaL_checkint(L,1);
    int right=luaL_checkint(L,2);
    lua_pushinteger(L,left >> right);
    return 1;
}

static int luaone_lshift(lua_State *L)
{
    int left=luaL_checkint(L,1);
    int right=luaL_checkint(L,2);
    lua_pushinteger(L,left << right);
    return 1;
}

static int luaone_readdir(lua_State *L)
{
    void *userdata  = luaL_checkudata(L,1,NYAOS_OPENDIR);
    DIR  *dirhandle = *static_cast<DIR**>( userdata );
    struct dirent *dirent;

    if( dirhandle == NULL )
        return 0;
    
    if( (dirent = readdir(dirhandle)) == NULL ){
        closedir(dirhandle);
        *(DIR**)userdata = NULL;
        return 0;
    }
    lua_pop(L,1);
    lua_pushstring(L,dirent->d_name);
    return 1;
}

static int luaone_closedir(lua_State *L)
{
    void *userdata = luaL_checkudata(L,1,NYAOS_OPENDIR);
    DIR *dirhandle = *static_cast<DIR**>(userdata) ;

    if( dirhandle != NULL )
        closedir(dirhandle);
    
    return 0;
}

static int luaone_opendir(lua_State *L)
{
    const char *dir = luaL_checkstring(L,1);
    DIR *dirhandle;
    void *userdata=NULL;

    dirhandle=opendir(dir);
    lua_pop(L,1);

    lua_pushcfunction(L,luaone_readdir); /* stack:1 */
    userdata = lua_newuserdata(L,sizeof(dirhandle)); /* stack:2 */
    if( userdata == NULL )
        return 0;
    memcpy( userdata , &dirhandle , sizeof(dirhandle) );

    /* create metatable for closedir */
    luaL_newmetatable(L,NYAOS_OPENDIR); /* stack:3 */
    lua_pushstring(L,"__gc");
    lua_pushcfunction(L,luaone_closedir);
    lua_settable(L,3);
    lua_setmetatable(L,2);

    return 2;
}

static struct luaone_s {
    const char *name;
    int (*func)(lua_State *);
} luaone[] = {
    { "chdir" , luaone_chdir } ,
    { "dir"   , luaone_opendir },
    { "bitand", luaone_bitand },
    { "bitor" , luaone_bitor },
    { "bitxor", luaone_bitxor },
    { "rshift", luaone_rshift },
    { "lshift", luaone_lshift },
    { NULL    , NULL } ,
};

int open_luautil( lua_State *L )
{
    lua_newtable(L);
    for(struct luaone_s *p=luaone ; p->name != NULL ; p++ ){
        lua_pushcfunction(L,p->func);
        lua_setfield(L,-2,p->name);
    }
    return 1;
}

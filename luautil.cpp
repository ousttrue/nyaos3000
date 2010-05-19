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
    int left=lua_tointeger(L,1);
    int right=lua_tointeger(L,2);
    if( !lua_isnumber(L,1) || ! lua_isnumber(L,1) ){
        lua_pushstring(L,"not number");
        lua_error(L);
    }
    lua_pushinteger(L,left & right);
    return 1;
}

static int luaone_bitor(lua_State *L)
{
    int left=lua_tointeger(L,1);
    int right=lua_tointeger(L,2);
    if( !lua_isnumber(L,1) || ! lua_isnumber(L,1) ){
        lua_pushstring(L,"not number");
        lua_error(L);
    }
    lua_pushinteger(L,left | right);
    return 1;
}

static int luaone_bitxor(lua_State *L)
{
    int left=lua_tointeger(L,1);
    int right=lua_tointeger(L,2);
    if( !lua_isnumber(L,1) || ! lua_isnumber(L,1) ){
        lua_pushstring(L,"not number");
        lua_error(L);
    }
    lua_pushinteger(L,left ^ right);
    return 1;
}

static int luaone_rshift(lua_State *L)
{
    int left=lua_tointeger(L,1);
    int right=lua_tointeger(L,2);
    if( !lua_isnumber(L,1) || ! lua_isnumber(L,1) ){
        lua_pushstring(L,"not number");
        lua_error(L);
    }
    lua_pushinteger(L,left >> right);
    return 1;
}

static int luaone_lshift(lua_State *L)
{
    int left=lua_tointeger(L,1);
    int right=lua_tointeger(L,2);
    if( !lua_isnumber(L,1) || ! lua_isnumber(L,1) ){
        lua_pushstring(L,"not number");
        lua_error(L);
    }
    lua_pushinteger(L,left << right);
    return 1;
}

static int luaone_readdir(lua_State *L)
{
    void *userdata;
    DIR  *dirhandle;
    struct dirent *dirent;

    if( (userdata  = lua_touserdata(L,1)) == NULL ||
        (dirhandle = *(DIR**)userdata)      == NULL ){
        return 0;
    }
    if( (dirent    = readdir(dirhandle)) == NULL ){
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
    void *userdata;
    DIR *dirhandle;

    if( (userdata=lua_touserdata(L,1)) != NULL &&
        (dirhandle = *(DIR**)userdata)   != NULL )
    {
        closedir(dirhandle);
    }
    return 0;
}

static int luaone_opendir(lua_State *L)
{
    const char *dir = lua_tostring(L,1);
    DIR *dirhandle;
    void *userdata=NULL;

    if( dir == NULL )
        return 0;

    dirhandle=opendir(dir);
    lua_pop(L,1);

    lua_pushcfunction(L,luaone_readdir); /* stack:1 */
    userdata = lua_newuserdata(L,sizeof(dirhandle)); /* stack:2 */
    if( userdata == NULL )
        return 0;
    memcpy( userdata , &dirhandle , sizeof(dirhandle) );

    /* create metatable for closedir */
    lua_newtable(L); /* stack:3 */
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

/*  nyaos.util = require("lueutil")
 */
int open_luautil( lua_State *L )
{
    lua_newtable(L);
    for(struct luaone_s *p=luaone ; p->name != NULL ; p++ ){
        lua_pushstring(L,p->name);
        lua_pushcfunction(L,p->func);
        lua_settable(L,-3);
    }
    return 1;
}

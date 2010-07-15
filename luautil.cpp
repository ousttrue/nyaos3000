#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}
#include "nndir.h"

const static char NYAOS_OPENDIR[] = "nyaos_opendir";

static int luaone_chdir(lua_State *L)
{
    const char *newdir = luaL_checkstring(L,1);
    errno = 0;
    lua_pushinteger( L , chdir(newdir) );
    lua_pushstring( L , strerror(errno) );
    return 2;
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

static void NnDir2Lua(lua_State *L,NnDir &stat)
{
    lua_newtable(L);

    lua_pushstring(L,stat.name());
    lua_setfield(L,-2,"name");

    lua_pushboolean(L,stat.isReadOnly() );
    lua_setfield(L,-2,"readonly");
    
    lua_pushboolean(L,stat.isHidden() );
    lua_setfield(L,-2,"hidden");

    lua_pushboolean(L,stat.isSystem() );
    lua_setfield(L,-2,"system");

    lua_pushboolean(L,stat.isLabel() );
    lua_setfield(L,-2,"label");

    lua_pushboolean(L,stat.isDir() );
    lua_setfield(L,-2,"directory");

    lua_pushinteger(L,stat.size() );
    lua_setfield(L,-2,"size");

    const NnTimeStamp &stamp=stat.stamp();
    lua_pushinteger(L,stamp.second);
    lua_setfield(L,-2,"second");
    lua_pushinteger(L,stamp.minute);
    lua_setfield(L,-2,"minute");
    lua_pushinteger(L,stamp.hour);
    lua_setfield(L,-2,"hour");
    lua_pushinteger(L,stamp.day);
    lua_setfield(L,-2,"day");
    lua_pushinteger(L,stamp.month);
    lua_setfield(L,-2,"month");
    lua_pushinteger(L,stamp.year);
    lua_setfield(L,-2,"year");
}

struct dirinfo_s {
    NnDir *info;
    enum dirinfo_e { FILEFIND , OPENDIR } mode;
};

static int luaone_readdir(lua_State *L)
{
    struct dirinfo_s *dirinfo=static_cast<struct dirinfo_s *>(
            luaL_checkudata(L,1,NYAOS_OPENDIR)
            );

    if( dirinfo == NULL || dirinfo->info == NULL )
        return 0;
    
    if( ! dirinfo->info->more() ){
        delete dirinfo->info;
        dirinfo->info = NULL ;
        return 0;
    }
    if( dirinfo->mode == dirinfo_s::OPENDIR ){
        lua_pushstring( L,dirinfo->info->name() );
    }else{
        NnDir2Lua(L,*dirinfo->info);
    }
    dirinfo->info->next();
    return 1;
}

static int luaone_closedir(lua_State *L)
{
    struct dirinfo_s *dirinfo = static_cast<struct dirinfo_s *>(
            luaL_checkudata(L,1,NYAOS_OPENDIR) 
            );

    if( dirinfo != NULL && dirinfo->info != NULL ){
        delete dirinfo->info;
        dirinfo->info = NULL;
    }
    return 0;
}

static int luaone_opendir_core(lua_State *L,dirinfo_s::dirinfo_e mode)
{
    const char *dir = luaL_checkstring(L,1);

    lua_pushcfunction(L,luaone_readdir); /* stack:1 */
    struct dirinfo_s *userdata = static_cast<struct dirinfo_s *>(
            lua_newuserdata(L,sizeof(struct dirinfo_s))
            ); /* stack:2 */

    if( userdata == NULL )
        return 0;

    if( mode == dirinfo_s::OPENDIR ){
        NnString dir_; dir_ << dir << "\\*";

        userdata->info = new NnDir(dir_);
        userdata->mode = mode;
    }else{
        userdata->info = new NnDir(dir);
        userdata->mode = mode;
    }
    /* create metatable for closedir */
    luaL_newmetatable(L,NYAOS_OPENDIR); /* stack:3 */
    lua_pushcfunction(L,luaone_closedir);
    lua_setfield(L,-2,"__gc");
    lua_setmetatable(L,-2);

    return 2;
}

static int luaone_findfirst(lua_State *L)
{ return luaone_opendir_core(L,dirinfo_s::FILEFIND); }
static int luaone_opendir(lua_State *L)
{ return luaone_opendir_core(L,dirinfo_s::OPENDIR); }

static int luaone_stat(lua_State *L)
{
    const char *fname = luaL_checkstring(L,-1);
    errno = 0;
    NnDir stat(fname);
    if( stat.more() ){
        NnDir2Lua(L,stat);
        return 1;
    }else{
        return 0;
    }
}

static int luaone_mkdir(lua_State *L)
{
    const char *dirname = luaL_checkstring(L,-1);
    errno = 0;
    lua_pushinteger( L,mkdir( dirname ) );
    lua_pushstring( L,strerror(errno) );
    return 2;
}

static int luaone_rmdir(lua_State *L)
{
    const char *dirname = luaL_checkstring(L,-1);
    errno = 0;
    lua_pushinteger( L,rmdir( dirname ) );
    lua_pushstring( L,strerror(errno) );
    return 2;
}


static struct luaone_s {
    const char *name;
    int (*func)(lua_State *);
} luaone[] = {
    { "chdir" , luaone_chdir } ,
    { "dir"   , luaone_opendir },
    { "filefind" , luaone_findfirst },
    { "bitand", luaone_bitand },
    { "bitor" , luaone_bitor },
    { "bitxor", luaone_bitxor },
    { "rshift", luaone_rshift },
    { "lshift", luaone_lshift },
    { "stat"  , luaone_stat },
    { "mkdir" , luaone_mkdir },
    { "rmdir" , luaone_rmdir },
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

#include <assert.h>
#include "shell.h"

#ifdef LUA_ENABLE

#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include "nnhash.h"
#include "getline.h"
#include "nua.h"
#include "ntcons.h"

#ifdef NYACUS
#  include <windows.h>
#  include <wincon.h>
#else
#  include <signal.h>
#endif

//#define TRACE(X) ((X),fflush(stdout))
#define TRACE(X)

extern int open_luautil( lua_State *L );
static lua_State *nua=NULL;

static void lstop (lua_State *L, lua_Debug *ar) {
  (void)ar;  /* unused arg. */
  lua_sethook(L, NULL, 0, 0);
  luaL_error(L, "interrupted!");
}

#ifdef OS2EMX
static void handle_ctrl_c(int sig)
{
    lua_sethook(nua, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
    signal(sig,SIG_ACK);
}
#else
static BOOL WINAPI handle_ctrl_c(DWORD ctrlChar)
{
    if( CTRL_C_EVENT == ctrlChar){
        lua_sethook(nua, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
        return TRUE;
    }else{
        return FALSE;
    }
}
#endif

/* Lua を(必要であれば)初期化すると同時に、
 * NYAOS テーブル上のオブジェクトをスタックに積む
 *    field - nyaos 上のフィールド名。NULL の時は nyaos 自体を積む
 *    L     - luaState オブジェクト。NULL の時は、当関数で取得する。
 * return
 *    not NULL - luaState オブジェクト
 *    NULL     - 初期化失敗 or nyaos がテーブルでなかった
 */
NyaosLua::NyaosLua( const char *field ) : NnLua()
{
    if( init() ){
        ready = 0;
        return;
    }
    lua_getglobal(L,"nyaos"); /* +1 */
    assert( lua_istable(L,-1) ); 

    if( field != NULL && field[0] != '\0' ){
        lua_getfield(L,-1,field);
        lua_remove(L,-2); /* drop 'nyaos' */
    }
    ready = 1;
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

int nua_history_add(lua_State *lua)
{
    History **history=(History**)lua_touserdata(lua,-2);
    const char *val=lua_tostring(lua,-1);
    if( history == NULL || val == NULL ){
        return 0;
    }
    **history << val;
    return 0;
}

int nua_history_drop(lua_State *lua)
{
    History **history=(History**)lua_touserdata(lua,-1);
    if( history == NULL )
        return 0;

    (*history)->drop();
    return 0;
}

int nua_history_get(lua_State *lua)
{
    History **history=(History**)lua_touserdata(lua,-2);
    int key=lua_tointeger(lua,-1);

    if( history == NULL )
        return 0;

    if( !lua_isnumber(lua,-1) ){
        const char *key=lua_tostring(lua,-1);
        if( key != NULL ){
            if( strcmp(key,"add") == 0 ){
                lua_pushcfunction(lua,nua_history_add);
                return 1;
            }else if( strcmp(key,"drop") == 0 ){
                lua_pushcfunction(lua,nua_history_drop);
                return 1;
            }
        }
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

int nua_history_set(lua_State *lua)
{
    History **history=(History**)lua_touserdata(lua,-3);
    int key=lua_tointeger(lua,-2);
    const char *val=lua_tostring(lua,-1);
    if( history == NULL || !lua_isnumber(lua,-2) ){
        lua_pushstring(lua,"invalid key index");
        lua_error(lua);
        return 0;
    }
    NnString rightvalue(val != NULL ? val : "" );
    (*history)->set(key-1,rightvalue);
    /* printf("[%d]='%s'\n",key-1,rightvalue.chars()); */
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

int nua_exec(lua_State *lua)
{
    const char *statement = lua_tostring(lua,-1);
    if( statement == NULL )
        return luaL_argerror(lua,1,"invalid nyaos-statement");
    
    OneLineShell shell( statement );

    shell.mainloop();

    lua_pushinteger(lua,shell.exitStatus());
    return 1;
}

int nua_access(lua_State *lua)
{
    const char *fname = lua_tostring(lua,1);
    int amode = lua_tointeger(lua,2);

    if( fname == NULL )
        return luaL_argerror(lua,2,"invalid filename");
    
    lua_pushboolean(lua, access(fname,amode)==0 );
    return 1;
}

int nua_getkey(lua_State *lua)
{
    char key=Console::getkey();
    lua_pushlstring(lua,&key,1);
    return 1;
}

int NyaosLua::initialized=0;

/* NYAOS 向け Lua 環境初期化
 * return
 *    0  : 成功 
 *    !0 : 失敗
 */
int NyaosLua::init()
{
    if( ! initialized ){
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

        open_luautil(L); 

        /* nyaos.command[] */
        lua_newtable(L);
        lua_setfield(L,-2,"command");

        /* table-like objects */
        while( p->name != NULL ){
            NnHash **u=(NnHash**)lua_newuserdata(L,sizeof(NnHash *));
            *u = p->dict;

            lua_newtable(L); /* metatable */
            if( p->index != NULL ){
                lua_pushcfunction(L,p->index);
                lua_setfield(L,-2,"__index");
            }
            if( p->newindex != NULL ){
                lua_pushcfunction(L,p->newindex);
                lua_setfield(L,-2,"__newindex");
            }
            if( p->call != NULL ){
                lua_pushcfunction(L,p->call);
                lua_setfield(L,-2,"__call");
            }
            lua_setmetatable(L,-2);

            lua_setfield(L,-2,p->name);
            p++;
        }
        History **h=(History**)lua_newuserdata(L,sizeof(History*));
        *h = &GetLine::history;

        /* history object */
        lua_newtable(L);
        lua_pushcfunction(L,nua_history_get);
        lua_setfield(L,-2,"__index");
        lua_pushcfunction(L,nua_history_len);
        lua_setfield(L,-2,"__len");
        lua_pushcfunction(L,nua_history_iter_factory);
        lua_setfield(L,-2,"__call");
        lua_pushcfunction(L,nua_history_set);
        lua_setfield(L,-2,"__newindex");
        lua_setmetatable(L,-2);
        lua_setfield(L,-2,"history");
        
        lua_pushcfunction(L,nua_exec);
        lua_setfield(L,-2,"exec");
        lua_pushcfunction(L,nua_access);
        lua_setfield(L,-2,"access");
        lua_pushcfunction(L,nua_getkey);
        lua_setfield(L,-2,"getkey");

        /* close nyaos table */
        lua_setglobal(L,"nyaos");

        initialized = 1;
    }
    return initialized ? 0 : -1;
}
#endif /* defined(LUA_ENABLED) */

int cmd_lua_e( NyadosShell &shell , const NnString &argv )
{
#ifdef LUA_ENABLE
    NnString arg1,left;
    argv.splitTo( arg1 , left );
    NyadosShell::dequote( arg1 );

    if( arg1.empty() ){
        conErr << "lua_e \"lua-code\"\n" ;
        return 0;
    }
    NyaosLua nua;

    /* 標準出力のリダイレクト・パイプ出力に対応 */
    int cur_out=+1;
    StreamWriter *sw=dynamic_cast<StreamWriter*>( conOut_  );
    if( sw != NULL ){
        cur_out = sw->fd();
    }else{
        RawWriter *rw=dynamic_cast<RawWriter*>( conOut_ );
        if( rw != NULL ){
            cur_out = rw->fd();
        }else{
            conErr << "CAN NOT GET HANDLE(STDOUT)\n";
        }
    }
    fflush(stdout);
    int back_out=-1;
    if( cur_out != 1 ){
        back_out = ::dup(1);
        ::dup2( cur_out , 1 );
    }
    /* 標準エラー出力のリダイレクト・パイプ出力に対応 */
    int cur_err=+2;
    sw=dynamic_cast<StreamWriter*>( &conErr );
    if( sw != NULL ){
        cur_err = sw->fd();
    }else{
        RawWriter *rw=dynamic_cast<RawWriter*>( &conErr );
        if( rw != NULL ){
            cur_err = rw->fd();
        }else{
            conErr << "Redirect and pipeline for stderr are not supported yet.\n";
        }
    }
    fflush(stderr);
    int back_err=-1;
    if( cur_err != 2 ){
        back_err = ::dup(2);
        ::dup2( cur_err , 2 );
    }

#ifdef OS2EMX
    signal( SIGINT , handle_ctrl_c );
#else
    Console::enable_ctrl_c();
    SetConsoleCtrlHandler( handle_ctrl_c , TRUE );
#endif

    /* Lua インタプリタコール */
    if( luaL_loadstring(nua,luaL_gsub( nua, arg1.chars(), "$T", "\n") ) ||
        lua_pcall( nua , 0 , 0 , 0 ) )
    {
        const char *msg = lua_tostring( nua , -1 );
        conErr << msg << '\n';
    }
    lua_settop(nua,0);

#ifdef OS2EMX
    signal( SIGINT , SIG_IGN );
#else
    SetConsoleCtrlHandler( handle_ctrl_c , FALSE );
    Console::disable_ctrl_c();
#endif

    /* 標準エラー出力を元に戻す */
    if( back_err >= 0 ){
        ::dup2( back_err , 2 );
        ::close( back_err );
    }
    /* 標準出力を元に戻す */
    if( back_out >= 0 ){
        ::dup2( back_out , 1 );
        ::close( back_out );
    }
#else
    conErr << "require: built-in lua disabled.\n";
#endif /* defined(LUA_ENABLED) */
    return 0;
}

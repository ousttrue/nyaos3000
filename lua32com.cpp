#include <stdio.h>

extern "C" {
#  include "lua.h"
#  include "lualib.h"
#  include "lauxlib.h"
}

#include "win32com.h"

//#define TRACE(x) puts(x)
#define TRACE(x)

static int destroy_object(lua_State *L)
{
    TRACE("[CALL] destroy_object");
    ActiveXObject **u=static_cast<ActiveXObject**>( lua_touserdata(L,1) );
    delete *u;
    return 0;
}

static int destroy_member(lua_State *L)
{
    TRACE("[CALL] destroy_member");
    ActiveXMember **m=static_cast<ActiveXMember**>(lua_touserdata(L,1));
    delete *m;
    return 0;
}

static int find_member(lua_State *L);
static void push_activexobject(lua_State *L,ActiveXObject *obj)
{
    ActiveXObject **u=
        static_cast<ActiveXObject**>(
                lua_newuserdata( L, sizeof(ActiveXObject*) ));
    *u = obj;
    luaL_newmetatable(L,"ActiveXObject");
    lua_pushcfunction(L,destroy_object);
    lua_setfield(L,-2,"__gc");
    lua_pushcfunction(L,find_member);
    lua_setfield(L,-2,"__index");
    lua_setmetatable(L,-2);
}

static int call_member(lua_State *L)
{
    TRACE("[CALL] call_member");
    Variants args;
    VARIANT result; VariantInit(&result);

    ActiveXMember **m=static_cast<ActiveXMember**>(
            luaL_checkudata(L,1,"ActiveXMember")); 
    if( m == NULL || !(**m).ok() ){
        lua_pushnil(L);
        lua_pushstring(L,"Invalid ActiveXMethod");
        return 2;
    }
    ActiveXObject **o=static_cast<ActiveXObject**>(
            luaL_checkudata(L,2,"ActiveXObject"));
    if( o == NULL || !(**o).ok() ){
        lua_pushnil(L);
        lua_pushstring(L,"Invalid ActiveXObject");
        return 2;
    }
    int n=lua_gettop(L);
    for(int i=3;i<=n;++i){
        args << lua_tostring(L,i);
    }
    HRESULT hr=(**m).invoke( DISPATCH_METHOD , args , args.size() , result );
    if( FAILED(hr) ){
        lua_pushnil(L);
        lua_pushstring(L,"Something happend on COM");
        return 2;
    }
    if( result.vt == VT_BSTR ){
        char *p=Unicode::b2c( result.bstrVal );
        // printf("RESULT=[%s](VT_BSTR)\n",p);
        lua_pushstring(L,p);
        delete[]p;
        return 1;
    }else if( result.vt == (VT_BSTR|VT_BYREF) ){
        char *p=Unicode::b2c( *result.pbstrVal );
        // printf("RESULT=[%s](VT_BSTR|VT_BYREF)\n",p);
        lua_pushstring(L,p);
        delete[]p;
        return 1;
    }else if( result.vt == VT_DISPATCH ){
        ActiveXObject *o=new ActiveXObject( result.pdispVal );
        push_activexobject(L,o);
        return 1;
    }else if( result.vt == (VT_DISPATCH|VT_BYREF) ){
        ActiveXObject *o=new ActiveXObject( *result.ppdispVal );
        push_activexobject(L,o);
        return 1;
    }else{
        // printf("result.vt == %d\n",result.vt);
        return 0;
    }
}

static int find_member(lua_State *L)
{
    TRACE("[CALL] find_member");
    ActiveXObject **u=static_cast<ActiveXObject**>(
            luaL_checkudata(L,1,"ActiveXObject") );
    if( u == NULL || !(**u).ok() ){
        lua_pushnil(L);
        lua_pushstring(L,"Invalid Object. Expected <ActiveXObject>");
        return 2;
    }
    const char *member_name=lua_tostring(L,2);
    ActiveXMember *member=new ActiveXMember(**u,member_name);
    if( member == NULL || !member->ok() ){
        lua_pushnil(L);
        lua_pushstring(L,"Can not find method for ActiveXObject");
        return 2;
    }
    ActiveXMember **m=static_cast<ActiveXMember**>(
            lua_newuserdata(L,sizeof(ActiveXMember*))
            );
    *m = member;
    luaL_newmetatable(L,"ActiveXMember");
    lua_pushcfunction(L,destroy_member);
    lua_setfield(L,-2,"__gc");
    lua_pushcfunction(L,call_member);
    lua_setfield(L,-2,"__call");
    lua_setmetatable(L,-2);
    return 1;
}


int create_object(lua_State *L)
{
    TRACE("[CALL] create_object");
    const char *name=lua_tostring(L,1);
    ActiveXObject *obj=new ActiveXObject(name);
    if( obj != NULL && !obj->ok() ){
        lua_pushnil(L);
        lua_pushstring(L,"Can not find ActiveXObject");
        return 2;
    }
    push_activexobject(L,obj);
    return 1;
}

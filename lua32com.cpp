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
    ActiveXObject **u=static_cast<ActiveXObject**>(
            luaL_checkudata(L,1,"ActiveXObject") );
    delete *u;
    return 0;
}

static int destroy_member(lua_State *L)
{
    TRACE("[CALL] destroy_member");
    ActiveXMember **m=static_cast<ActiveXMember**>(
            luaL_checkudata(L,1,"ActiveXMember"));
    delete *m;
    return 0;
}

static void lua2variants( lua_State *L , int i , Variants &args )
{
    switch( lua_type(L,i) ){
    case LUA_TNUMBER:
        args << lua_tonumber(L,i);
        break;
    case LUA_TBOOLEAN:
    case LUA_TNIL:
        args.add_as_boolean( lua_toboolean(L,i) );
        break;
    case LUA_TSTRING:
    default:
        args << lua_tostring(L,i);
        break;
    }
}

static int put_property(lua_State *L)
{
    TRACE("[CALL] put_property");
    ActiveXObject **u=static_cast<ActiveXObject**>(
            luaL_checkudata(L,1,"ActiveXObject") );
    if( u == NULL ){
        TRACE("no ActiveXObject");
        return 0;
    }
    const char *member_name = lua_tostring(L,2);
    if( member_name == NULL ){
        TRACE("no member name");
        return 0;
    }

    Variants args;  lua2variants(L,3,args);
    VARIANT result; VariantInit(&result);

    (**u).invoke( member_name , DISPATCH_PROPERTYPUT , args , args.size() , result );
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
    lua_pushcfunction(L,put_property);
    lua_setfield(L,-2,"__newindex");
    lua_setmetatable(L,-2);
}

static int variant2lua( VARIANT &v , lua_State *L )
{
    if( v.vt == VT_BSTR ){
        char *p=Unicode::b2c( v.bstrVal );
        // printf("v=[%s](VT_BSTR)\n",p);
        lua_pushstring(L,p);
        delete[]p;
    }else if( v.vt == (VT_BSTR|VT_BYREF) ){
        char *p=Unicode::b2c( *v.pbstrVal );
        // printf("v=[%s](VT_BSTR|VT_BYREF)\n",p);
        lua_pushstring(L,p);
        delete[]p;
    }else if( v.vt == VT_DISPATCH ){
        ActiveXObject *o=new ActiveXObject( v.pdispVal );
        push_activexobject(L,o);
    }else if( v.vt == (VT_DISPATCH|VT_BYREF) ){
        ActiveXObject *o=new ActiveXObject( *v.ppdispVal );
        push_activexobject(L,o);
    }else if( v.vt == VT_BOOL ){
        lua_pushboolean(L,v.boolVal);
    }else if( v.vt == (VT_BOOL|VT_BYREF) ){
        lua_pushboolean(L,*v.pboolVal); 
    }else if( v.vt == VT_UI1 ){
        lua_pushinteger(L,v.bVal);
    }else if( v.vt == (VT_UI1|VT_BYREF) ){
        lua_pushinteger(L,*v.pbVal);
    }else if( v.vt == VT_I2 ){
        lua_pushinteger(L,v.iVal);
    }else if( v.vt == (VT_I2|VT_BYREF) ){
        lua_pushinteger(L,*v.piVal);
    }else if( v.vt == VT_I4 ){
        lua_pushinteger(L,v.lVal);
    }else if( v.vt == (VT_I4|VT_BYREF) ){
        lua_pushinteger(L,*v.plVal);
    }else if( v.vt == VT_R4 ){
        lua_pushnumber(L,v.fltVal);
    }else if( v.vt == (VT_R4|VT_BYREF) ){
        lua_pushnumber(L,*v.pfltVal);
    }else if( v.vt == VT_R8 ){
        lua_pushnumber(L,v.dblVal);
    }else if( v.vt == (VT_R8|VT_BYREF) ){
        lua_pushnumber(L,*v.pdblVal);
    }else{
        // printf("vt=[%d]\n",v.vt);
        return 0;
    }
    return 1;
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
    for(int i=lua_gettop(L);i>=3;--i){
        lua2variants(L,i,args);
    }
    HRESULT hr=(**m).invoke( DISPATCH_METHOD | DISPATCH_PROPERTYGET, args , args.size() , result );
    if( FAILED(hr) ){
        lua_pushnil(L);
        lua_pushstring(L,"Something happend on COM");
        return 2;
    }
    return variant2lua(result,L);
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

    Variants args;
    VARIANT result; VariantInit(&result);
    HRESULT hr=member->invoke( DISPATCH_PROPERTYGET , args , 0 , result );
    if( FAILED(hr) ){
        // member is method 
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
    }else{
        // member is property
        int rc=variant2lua( result , L );
        delete member;
        return rc;
    }
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

#include <stdio.h>

extern "C" {
#  include "lua.h"
#  include "lualib.h"
#  include "lauxlib.h"
}

#include "win32com.h"

// #define DBG(x) (x)
#define DBG(x)

static void hr_to_lua_message(HRESULT hr,lua_State *L)
{
    LPVOID s;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM ,
                  NULL ,
                  hr ,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR)&s,
                  0,
                  NULL);
    lua_pushstring(L,(LPCTSTR)s);
    LocalFree(s);
}

static void set_nyaos_error(lua_State *L,const char *s)
{
    lua_getglobal(L,"nyaos");
    if( lua_istable(L,-1) ){
        if( s != NULL ){
            lua_pushstring(L,s);
        }else{
            lua_pushnil(L);
        }
        lua_setfield(L,-2,"error");
    }
    lua_pop(L,1);
}

static int destroy_object(lua_State *L)
{
    DBG( puts("[CALL] destroy_object") );
    ActiveXObject **u=static_cast<ActiveXObject**>(
            luaL_checkudata(L,1,"ActiveXObject") );
    delete *u;
    return 0;
}

static int destroy_member(lua_State *L)
{
    DBG( puts("[CALL] destroy_member") );
    ActiveXMember **m=static_cast<ActiveXMember**>(
            luaL_checkudata(L,1,"ActiveXMember"));
    delete *m;
    return 0;
}

static void lua2variants( lua_State *L , int i , Variants &args )
{
    switch( lua_type(L,i) ){
    case LUA_TTABLE:
        {
            int n=luaL_len(L,i);
            DBG( printf("[lua2variants] stack=%d (before)\n",lua_gettop(L)));
            const char **array=(const char**)malloc(sizeof(const char*)*n);
            for(int j=0;j<n;j++){
                lua_pushinteger(L,j+1);
                lua_gettable(L,i);
                array[j] = lua_tostring(L,-1);
                DBG( printf("array[%d]=%s\n",j,array[j]) );
                lua_pop(L,1);
            }
            DBG( printf("[lua2variants] stack=%d (after)\n",lua_gettop(L)));
            args.add_str_array( n , array );
            free(array);
        }
        break;
    case LUA_TNUMBER:
        args.add_as_number( lua_tonumber(L,i) );
        break;
    case LUA_TBOOLEAN:
        args.add_as_boolean( lua_toboolean(L,i) );
        break;
    case LUA_TNIL:
        args.add_as_null();
        break;
    case LUA_TSTRING:
    default:
        args.add_as_string( lua_tostring(L,i) );
        break;
    }
}

static int put_property(lua_State *L)
{
    DBG( puts("[CALL] put_property") );
    ActiveXObject **u=static_cast<ActiveXObject**>(
            luaL_checkudata(L,1,"ActiveXObject") );
    if( u == NULL ){
        DBG( puts("no ActiveXObject") );
        return 0;
    }
    const char *member_name = lua_tostring(L,2);
    if( member_name == NULL ){
        DBG( puts("no member name") );
        return 0;
    }

    Variants args;  lua2variants(L,3,args);
    VARIANT result; VariantInit(&result);

    (**u).invoke( member_name , DISPATCH_PROPERTYPUT , args , args.size() , result );
    return 0;
}

static int return_self(lua_State *L)
{
    DBG( printf("[CALL] return_self argc=%d\n",lua_gettop(L)) );
    DBG( printf("lua_type(argc[-1])==%d\n",lua_type(L,-1)) );
    lua_pushvalue(L,1);
    return 1;
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
    lua_pushcfunction(L,return_self);
    lua_setfield(L,-2,"__call");
    lua_setmetatable(L,-2);
}

static int variant2lua( VARIANT &v , lua_State *L )
{
    if( v.vt == VT_BSTR ){
        char *p=Unicode::b2c( v.bstrVal );
        lua_pushstring(L,p);
        delete[]p;
    }else if( v.vt == (VT_BSTR|VT_BYREF) ){
        char *p=Unicode::b2c( *v.pbstrVal );
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
        DBG( printf("vt=[%d]\n",v.vt) );
        return 0;
    }
    return 1;
}

static int call_member(lua_State *L)
{
    DBG( puts("[CALL] call_member") );
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
    char *error_info=0;
    if( (**m).invoke( DISPATCH_METHOD | DISPATCH_PROPERTYGET ,
                args , args.size() , result , NULL , &error_info ) != 0 )
    {
        lua_pushnil(L);
        if( error_info != 0 ){
            lua_pushstring(L,error_info);
            delete[]error_info;
            set_nyaos_error(L,error_info);
        }else{
            lua_pushstring(L,"Something happend on COM");
            set_nyaos_error(L,"Something happend on COM");
        }
        return 2;
    }
    DBG( puts("invoke(DISPATCH_PROPERTYGET|DISPATCH_METHOD) succeded.") );
    delete[]error_info;
    return variant2lua(result,L);
}

static void push_activexmember(lua_State *L,ActiveXMember *member)
{
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
}

static int find_member(lua_State *L)
{
    DBG( puts("[CALL] find_member") );
    ActiveXObject **u=static_cast<ActiveXObject**>(
            luaL_checkudata(L,1,"ActiveXObject") );
    if( u == NULL || !(**u).ok() ){
        set_nyaos_error(L,"Invalid Object. Expected <ActiveXObject>");
        lua_pushnil(L);
        lua_pushstring(L,"Invalid Object. Expected <ActiveXObject>");
        return 2;
    }
    const char *member_name=lua_tostring(L,2);
    ActiveXMember *member=new ActiveXMember(**u,member_name);
    if( member == NULL ){
        lua_pushnil(L);
        lua_pushstring(L,"Can not find method for ActiveXObject");
        return 2;
    }else if( !member->ok() ){
        lua_pushnil(L);
        hr_to_lua_message(member->construct_error(),L);
        set_nyaos_error(L,lua_tostring(L,-1));
        delete member;
        return 2;
    }

    Variants args;
    VARIANT result; VariantInit(&result);
    HRESULT hr;
    char *error_info=0;
    int rc=0;
    if( member->invoke( DISPATCH_PROPERTYGET ,
                args , 0 , result , &hr , &error_info ) != 0 )
    {
        DBG( printf("invoke('%s',DISPATCH_PROPERTYGET) failed. hr==%0lX\n",
                    member_name,hr) );

        if( hr == DISP_E_MEMBERNOTFOUND || hr == DISP_E_BADPARAMCOUNT ){
            DBG( puts("member is method") );
            push_activexmember(L,member);
            rc = 1;
        }else{
            delete member;
            if( error_info != 0 ){
                set_nyaos_error(L,error_info);
                lua_pushnil(L);
                lua_pushstring(L,error_info);
            }else{
                lua_pushnil(L);
                hr_to_lua_message(hr,L);
            }
            set_nyaos_error(L,lua_tostring(L,-1));
            rc = 2;
        }
    }else{
        DBG( printf("invoke('%s',DISPATCH_PROPERTYGET) success. hr==%0lX\n",
                    member_name,hr) );
        rc=variant2lua( result , L );
        if( rc != 0 ){
            // member is property
            delete member;
        }else{
            // member is method
            push_activexmember(L,member);
            rc = 1;
        }
    }
    delete[]error_info;
    if( result.vt != VT_DISPATCH ){
        VariantClear( &result );
    }
    return rc;
}

static int new_activex_object(lua_State *L,bool isNewObject)
{
    DBG( puts("[CALL] create_object") );
    const char *name=lua_tostring(L,1);
    ActiveXObject *obj=new ActiveXObject(name,isNewObject);
    if( obj == NULL ){
        lua_pushnil(L);
        lua_pushstring(L,"Can not find ActiveXObject");
        return 2;
    }else if( !obj->ok() ){
        lua_pushnil(L);
        hr_to_lua_message(obj->construct_error(),L);
        delete obj;
        return 2;
    }else{
        push_activexobject(L,obj);
        return 1;
    }
}

int com_create_object(lua_State *L)
{
    return new_activex_object(L,true);
}

int com_get_active_object(lua_State *L)
{
    return new_activex_object(L,false);
}

#include "win32com.h"

//#define DEBUG

#include <stdlib.h>

#ifdef DEBUG
#  include <stdio.h>
#endif

BSTR Unicode::c2b(const char *s)
{
    size_t csize=strlen(s);
    size_t wsize=MultiByteToWideChar(CP_ACP,MB_PRECOMPOSED,s,csize,NULL,0);
    BSTR w=SysAllocStringLen(NULL,wsize );
    if( w == NULL ){
#ifdef DEBUG
        puts("memory allocation error");
#endif
        return NULL;
    }
    MultiByteToWideChar(CP_ACP,0,s,csize,w,wsize);
    return w;
}

char *Unicode::b2c(BSTR w)
{
    size_t csize=WideCharToMultiByte(CP_ACP,0,(OLECHAR*)w,-1,NULL,0,NULL,NULL);
    char *p=new char[ csize+1 ];
    WideCharToMultiByte(CP_ACP,0,(OLECHAR*)w,-1,p,csize,NULL,NULL);
    return p;
}

class UnicodeStack : public Unicode {
    UnicodeStack *next;
public:
    UnicodeStack( const char *s , UnicodeStack *n ) : Unicode(s) , next(n){}
    ~UnicodeStack(){ delete next; }
};

Variants::~Variants()
{
    free(v);
    delete str_stack;
}

int ActiveXObject::instance_count = 0;

ActiveXObject::~ActiveXObject()
{
    if( pApplication != NULL )
        pApplication->Release();
    if( --instance_count <= 0 )
        CoUninitialize();
}

ActiveXObject::ActiveXObject(const char *name,bool isNewInstance)
{
    CLSID clsid;

    this->pApplication = NULL;
    construct_error_ = 0;

    if( instance_count++ <= 0 )
        CoInitialize(NULL);

    Unicode className(name);

    /* クラスID取得 */
    construct_error_ = CLSIDFromProgID( className , &clsid );
    if( FAILED(construct_error_) ){
#ifdef DEBUG
        puts("Error: CLSIDFromProgID");
#endif
        return;
    }

    if( isNewInstance ){ // create_object
        /* インスタンス作成 */
        construct_error_ = CoCreateInstance(
                clsid ,
                NULL ,
                CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER ,
                IID_IDispatch ,
                (void**)&this->pApplication );
        if( FAILED(construct_error_) ){
#ifdef DEBUG
            puts("Error: CoCreateInstance");
#endif
            if( pApplication != NULL ){
                pApplication->Release();
                pApplication = NULL;
            }
        }
    }else{ // get_object
        IUnknown *pUnknown=NULL;
        construct_error_ = GetActiveObject( clsid , 0 , &pUnknown );
        if( FAILED(construct_error_) ){
            if( pUnknown != NULL )
                pUnknown->Release();
            return;
        }
        construct_error_ = pUnknown->QueryInterface(
                IID_IDispatch ,
                (void**)&this->pApplication );
        if( pUnknown != NULL )
            pUnknown->Release();
        if( FAILED(construct_error_) ){
            if( pApplication != NULL ){
                pApplication->Release();
                pApplication = NULL;
            }
        }
    }
}

ActiveXMember::ActiveXMember( ActiveXObject &instance , const char *name ) : instance_(instance)
{
    Unicode methodName(name);

    this->construct_error_ = instance.getIDispatch()->GetIDsOfNames(
            IID_NULL , /* 将来のための予約フィールド */
            &methodName ,
            1 ,
            LOCALE_USER_DEFAULT , 
            &dispid_ );
}

int ActiveXMember::invoke(
        WORD wflags ,
        VARIANT *argv ,
        int argc , 
        VARIANT &result ,
        HRESULT *pHr ,
        char **error_info )
{
    HRESULT hr;
    DISPPARAMS disp_params;
    DISPID dispid_propertyput=DISPID_PROPERTYPUT;
    UINT puArgerr = 0;
    EXCEPINFO excepinfo;

    disp_params.cArgs  = argc ;
    disp_params.rgvarg = argv ;

    if( wflags & DISPATCH_PROPERTYPUT ){
        disp_params.cNamedArgs = 1;
        disp_params.rgdispidNamedArgs = &dispid_propertyput;
    }else{
        disp_params.cNamedArgs = 0;
        disp_params.rgdispidNamedArgs = NULL;
    }

    hr = this->instance_.getIDispatch()->Invoke(
            this->dispid() ,
            IID_NULL ,
            LOCALE_SYSTEM_DEFAULT ,
            wflags ,
            &disp_params ,
            &result ,
            &excepinfo ,
            &puArgerr );
    if( pHr != NULL ){
        *pHr = hr;
    }
    if( FAILED(hr) ){
        if( hr == DISP_E_EXCEPTION && error_info != 0 ){
            *error_info = Unicode::b2c( excepinfo.bstrDescription );
        }
        return -1;
    }else{
        return 0;
    }
}

int ActiveXObject::invoke(
        const char *name,
        WORD wflags ,
        VARIANT *argv ,
        int argc ,
        VARIANT &result )
{
    ActiveXMember method( *this , name );
    if( ! method.ok() )
        return -1;

    return method.invoke(wflags,argv,argc,result);
}

void Variants::grow()
{
    v = static_cast<VARIANTARG*>( realloc(v,++size_*sizeof(VARIANTARG)) );
}

void Variants::add_as_string(const char *s)
{
    grow();
    str_stack=new UnicodeStack(s,str_stack);

    VariantInit( &v[size_-1] );
    v[size_-1].vt = VT_BSTR;
    v[size_-1].bstrVal = *str_stack;
}

void Variants::add_as_number(double d)
{
    grow();

    VariantInit( &v[size_-1] );
    v[size_-1].vt     = VT_R8;
    v[size_-1].dblVal = d;
}

void Variants::add_as_boolean(int n)
{
    grow();

    VariantInit( &v[size_-1] );
    v[size_-1].vt = VT_BOOL;
    v[size_-1].boolVal = n ? VARIANT_TRUE : VARIANT_FALSE ;
}

void Variants::add_as_null()
{
    grow();

    VariantInit( &v[size_-1] );
    v[size_-1].vt = VT_NULL;
}

#ifndef WIN32COM_H
#define WIN32COM_H

#include <windows.h>

class Unicode {
    BSTR w;
public:
    static BSTR  c2b(const char *s); // To free, SysFreeString(r);
    static char *b2c(BSTR w); // To free, delete[]r;

    Unicode(const char *s) : w(c2b(s)){}
    ~Unicode(){ if(w){ SysFreeString(w); } }

    operator const BSTR &(){ return w; }
    BSTR *operator &(){ return &w; }
    char *toChar(){ return b2c(w); } // To free, delete[]r;
};
class UnicodeStack;

class Variants {
    VARIANTARG *v;
    int size_;
    UnicodeStack *str_stack;
    void grow();
public:
    Variants() : v(NULL),size_(0),str_stack(0){}
    ~Variants();

    operator VARIANTARG*(){ return v; }
    void operator << ( const char *s );
    void operator << ( int n );
    void operator << ( double d );
    void add_as_boolean( int n );
    size_t size(){ return size_; }
};

class ActiveXObject {
    static int instance_count;
    IDispatch *pApplication;
public:
    explicit ActiveXObject(const char *name,bool isNewInstance=true);
    explicit ActiveXObject(IDispatch *p) : pApplication(p) { instance_count++; }
    ~ActiveXObject();

    int invoke(const char *name,
            WORD wflags ,
            VARIANT *argv ,
            int argc ,
            VARIANT &result );

    int ok() const { return pApplication != NULL ; }

    IDispatch *getIDispatch(){ return pApplication; }
};

class ActiveXMember {
    ActiveXObject &instance_;
    DISPID dispid_;
    HRESULT construct_error;
public:
    ActiveXMember( ActiveXObject &instance , const char *name );
    DISPID dispid() const { return dispid_; }
    int invoke(WORD wflags,VARIANT *argv , int argc , VARIANT &result,HRESULT *hr=0,char **error_info=0 );

    int ok() const { return ! FAILED(construct_error); }
};

#endif

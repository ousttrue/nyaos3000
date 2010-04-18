#ifdef LUA_ENABLE

#include "nnlua.h"

class NyaosLua : public NnLua {
    private:
        static int initialized;
        int init();
        int ready;
    public:
        NyaosLua() : NnLua() , ready(1) { init(); }
        NyaosLua( const char *field );
        int ok() const { return L!=NULL && ready;  }
        int ng() const { return L==NULL || !ready; }
        operator lua_State*() { return L; }
};

#endif
/* vim:set ft=cpp textmode: */

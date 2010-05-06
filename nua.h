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

void redirect_emu_to_real(int &back_out,int &back_err);
void redirect_rewind(int back_out,int back_err);

#endif
/* vim:set ft=cpp textmode: */

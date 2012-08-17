#ifndef NNLUA_HEADER
#define NNLUA_HEADER

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

class NnLua{
    private:
        static void shutdown();
        int initlevel;
    protected:
        static lua_State *L;
    public:
        NnLua();
        ~NnLua();
        operator lua_State* () { return L; }
};

#endif
/* vim:set ft=cpp: */

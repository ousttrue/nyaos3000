#ifndef MYSYSTEM_H
#define MYSYSTEM_H

#include <windows.h>

#ifdef EMXOS2
    typedef int phandle_t;
#else
    typedef HANDLE phandle_t;
#endif

int  mySystem( const char *cmdline , int wait );
int  myPopen(const char *cmdline , const char *mode , phandle_t *pid );
void myPclose(int fd, phandle_t phandle );

#endif

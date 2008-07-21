@echo off
for %%i in (bcc sc os2 clean) do if (%%i) == (%1) goto %%i
    echo usage
    echo    %0 bcc   ... create Makefile for NYACUS   (Borland C++)
    echo    %0 sc    ... create Makefile for NYADOS   (Digitalmars C++)
    echo    %0 os2   ... create Makefile for NYAOS-II (emx/gcc)
    echo    %0 clean ... remove makefile.*
    pause
    goto bye
:bcc
    cscript //Nologo ifdoc.vbs NYACUS      < template.mak > Makefile
    goto bye
:sc
    cscript //Nologo ifdoc.vbs DIGITALMARS < template.mak > Makefile
    goto bye
:os2
    ifdoc.cmd NYAOS2                       < template.mak > Makefile
    goto bye
:win
    cscript //Nologo ifdoc.vbs NYACUS      < template.mak > Makefile.bcc
    cscript //Nologo ifdoc.vbs DIGITALMARS < template.mak > Makefile.sc
    goto bye
:clean
    if exist Makefile.bcc del Makefile.bcc
    if exist Makefile.sc  del Makefile.sc
    if exist Makefile.emx del Makefile.emx
    if exist Makefile.vc  del Makefile.vc
    if exist Makefile     del Makefile
:bye

@echo off
if not '%1' == '' goto main
    echo Usage: %0 VERSION-STRING
    goto exit
:main
if not '%OS%' == 'Windows_NT' goto OS2
    zip nyaos3k-%1-win.zip nyaos.exe _nya nyaos.txt
    hg archive -t zip nyaos3k-%1-src.zip
    goto exit
:OS2
    zip nyaos3k-%1-os2.zip nyaos.exe _nya nyaos.txt
:exit

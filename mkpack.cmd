@echo off
echo 0=%0
echo 1=%1
echo 2=%2
echo 3=%3
echo 4=%4
if not '%1' == '' goto main
    echo Usage: %0 VERSION-STRING
    goto exit
:main
if not '%OS%' == 'Windows_NT' goto OS2
    zip nyaos3k-%1-win.zip nyaos.exe _nya nyaos*.txt
    hg archive -t zip nyaos3k-%1-src.zip
    goto exit
:OS2
    zip nyaos3k-%1-os2.zip nyaos.exe _nya nyaos*.txt
:exit

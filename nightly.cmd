@echo off
if '%1' == '' (
    echo Usage: %0 VERSION-STRING
) else (
    cd ../
    zip nyaos3k-%1.zip ^
        nyaos3k/*.exe ^
        nyaos3k/Makefile ^
        nyaos3k/*.h ^
        nyaos3k/*.cpp ^
        nyaos3k/*.ico ^
        nyaos3k/*.rc  ^
        nyaos3k/*.m4 ^
        nyaos3k/_nya 
)

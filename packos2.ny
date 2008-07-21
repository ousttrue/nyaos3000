# This is NYAScript for OS/2 .
#
# suffix ny %NYATYPE% -f
# packos2.ny NNN (NNN:version number)

if /%1/ == // then 
    echo %NYATYPE% -f %0 NNN
    exit 0
else
    zip nyaos%1.zip nyaos2.txt nyaos2.exe nyaos2.ico _nya
endif

# This is NYAScript.
#
# suffix ny %NYATYPE% -f
# pack.ny NNN (NNN:version number)

if /%1/ == // then 
    echo %NYATYPE% -f %0 NNN
    exit 0
else
    source makedoc.ny
    if %NYATYPE% == NYAOS2 then
	zip nyaos%1.zip NYAOS2.TXT NYAOS2.EXE _nya
    else
	zip nyados%1.zip NYADOS.TXT NYADOS.EXE greencat.ico _nya
	zip nyacus%1.zip NYACUS.TXT NYACUS.EXE _nya tagjump.vbs
	zip nya%1.zip makemake.ny template.mak Makefile.* _nya makedoc.ny *.txt
	zip nya%1.zip document.erb pack.ny packos2.ny nyacus.rc
	zip nya%1.zip *.cpp ifdef.vbs ifdef.cmd 
	zip nya%1.zip makemake.cmd *.h *.ico
#	move nyados%1.zip \home\web\nnn\nya\.
#	move nyacus%1.zip \home\web\nnn\nya\.
#	move nya%1.zip    \home\web\nnn\nya\.
    endif
endif

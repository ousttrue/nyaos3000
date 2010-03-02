all:
	@echo "make"
	@echo "  print usage(this)"
	@echo "make nyaos3k"
	@echo "  build nyaos.exe with MinGW and Lua"
	@echo "make emxos2"
	@echo "  build nyaos2.exe with emx/gcc on OS/2 Warp"
	@echo "make clean"
	@echo "  clean *.obj *.o *.exe. Please ignore error message"
	@echo "make cleanobj"
	@echo "  clean *.obj *.o. Please ignore error message"
	@echo ""
	@echo "make documents"
	@echo "make release"

LUAPATH=../lua-5.1.4

.SUFFIXES : .cpp .obj .exe .h .res .rc .cpp .h .o
.cpp.obj :
	$(CC) $(CFLAGS) -c $<
.cpp.o :
	$(CC) $(CFLAGS) -c $<

CCC=-DNDEBUG

nyaos3k : 
	$(MAKE) CC=gcc CFLAGS="-Wall -O3 $(CCC) -I/usr/local/include -I$(LUAPATH)/src -mno-cygwin -D_MSC_VER=1000 -DLUA_ENABLE" O=o \
		LDFLAGS="-s -lole32 -luuid -llua -lstdc++ -L$(LUAPATH)/src -L/usr/lib/mingw/" \
		nyaos.exe

emxos2 :
	$(MAKE) CC=gcc NAME=NYAOS2 CFLAGS="-O2 -Zomf -Zsys -DOS2EMX $(CCC)" O=obj \
		LDFLAGS=-lstdcpp nyaos2.exe

lua :
	$(MAKE) -C $(LUAPATH) CC="gcc -mno-cygwin" generic

clean-lua :
	$(MAKE) -C $(LUAPATH) clean

OBJS=nyados.$(O) nnstring.$(O) nndir.$(O) twinbuf.$(O) mysystem.$(O) keyfunc.$(O) \
	getline.$(O) getline2.$(O) keybound.$(O) dosshell.$(O) nnhash.$(O) \
	writer.$(O) history.$(O) ishell.$(O) scrshell.$(O) wildcard.$(O) cmdchdir.$(O) \
	shell.$(O) shell4.$(O) foreach.$(O) which.$(O) reader.$(O) nnvector.$(O) \
	ntcons.$(O) shellstr.$(O) cmds1.$(O) cmds2.$(O) xscript.$(O) shortcut.$(O) \
	strfork.$(O) lsf.$(O) open.$(O) nua.$(O) luautil.$(O)

nyaos2.exe : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

nyaos.exe : $(OBJS) nyacusrc.$(O)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	objdump -x $@ | findstr "DLL Name"

# メインルーチン
nyados.$(O)   : nyados.cpp   nnstring.h getline.h

# 一行入力
twinbuf.$(O)  : twinbuf.cpp  getline.h
getline.$(O)  : getline.cpp  getline.h nnstring.h
getline2.$(O) : getline2.cpp getline.h
keybound.$(O) : keybound.cpp getline.h
keyfunc.$(O) : keyfunc.cpp getline.h
dosshell.$(O) : dosshell.cpp getline.h
xscript.$(O) : xscript.cpp

# インタプリタ処理
shell.$(O)    : shell.cpp    shell.h 
shell4.$(O)   : shell4.cpp   shell.h nnstring.h
scrshell.$(O) : scrshell.cpp shell.h
ishell.$(O)   : ishell.cpp   shell.h ishell.h 
mysystem.$(O) : mysystem.cpp nnstring.h
shellstr.$(O) : shellstr.cpp

# 個別コマンド処理
cmds1.$(O) : cmds1.cpp shell.h nnstring.h
cmds2.$(O) : cmds2.cpp shell.h nnstring.h
cmdchdir.$(O) : cmdchdir.cpp shell.h nnstring.h nndir.h
foreach.$(O) : foreach.cpp shell.h
lsf.$(O) : lsf.cpp

# 共通ライブラリ
nnstring.$(O)  : nnstring.cpp  nnstring.h  nnobject.h
nnvector.$(O)  : nnvector.cpp  nnvector.h  nnobject.h
nnhash.$(O) : nnhash.cpp nnhash.h nnobject.h
strfork.$(O) : strfork.cpp

# 環境依存ライブラリ
writer.$(O) : writer.cpp    writer.h
reader.$(O) : reader.cpp    reader.h
nndir.$(O) : nndir.cpp nndir.h
wildcard.$(O) : wildcard.cpp  nnstring.h nnvector.h nndir.h
ntcons.$(O) : ntcons.cpp
open.$(O) : open.cpp

nua.$(O) : nua.cpp
luautil.$(O) : luautil.cpp

# リソース
nyacusrc.$(O)  : nyacus.rc redcat.ico
	windres --output-format=coff -o $@ $<

documents : nyaos2.txt nyacus.txt

nyaos2.txt : nya.m4
	m4 -DSHELL=NYAOS2 $< > $@
nyacus.txt : nya.m4
	m4 -DSHELL=NYACUS $< > $@
clean : 
	del *.obj *.o *.exe || rm *.obj *.o *.exe *.EXE

cleanobj :
	del *.obj *.o || rm *.obj *.o

# vim:set noet ts=8 sw=8 nobk:

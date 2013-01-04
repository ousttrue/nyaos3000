# for static link
LUAPATH=../lua-5.2.1
L=$(LUAPATH)/src/

# for dynamic link
DLUAPATH=../lua-5.2_Win32_dllw4_lib

RM=cmd /c del

.SUFFIXES : .cpp .obj .exe .h .res .rc .cpp .h .o

.cpp.obj :
	$(CXX) $(CXXFLAGS) -c $<
.cpp.o :
	$(CXX) $(CXXFLAGS) -c $<

all :
ifeq ($(OS),Windows_NT)
	$(MAKE) CXXFLAGS="-O3 -Wall -DNYACUS -I$(LUAPATH)/src" O=o \
		LDFLAGS="-s -lole32 -loleaut32 -luuid -llua -static -L$(LUAPATH)/src" nyaos.exe

dynamic_link :
	$(MAKE) CXXFLAGS="-O3 -Wall -DNYACUS -I$(DLUAPATH)/include" O=o \
		LDFLAGS="-s -lole32 -loleaut32 -luuid -llua52 -static -L$(DLUAPATH)" nyaos.exe

lua :
	$(MAKE) -C $(L) generic MYCFLAGS="-fno-omit-frame-pointer"
else
	$(MAKE) CXXFLAGS="-O2 -I$(LUAPATH)/src -DNYAOS2 -Zomf -Zsys" O=obj LDFLAGS="$(L)lapi.o $(L)lcode.o $(L)lctype.o $(L)ldebug.o $(L)ldo.o $(L)ldump.o $(L)lfunc.o $(L)lgc.o $(L)llex.o $(L)lmem.o $(L)lobject.o $(L)lopcodes.o $(L)lparser.o $(L)lstate.o $(L)lstring.o $(L)ltable.o $(L)ltm.o $(L)lundump.o $(L)lvm.o $(L)lzio.o $(L)lauxlib.o $(L)lbaselib.o $(L)lbitlib.o $(L)lcorolib.o $(L)ldblib.o $(L)liolib.o $(L)lmathlib.o $(L)loslib.o $(L)lstrlib.o $(L)ltablib.o $(L)loadlib.o $(L)linit.o -lstdcpp" nyaos.exe

lua :
	$(MAKE) -C $(L) MYCFLAGS="-Zomf -Zsys -fno-omit-frame-pointer" o
endif


OBJS_=nyados.$(O) nnstring.$(O) nndir.$(O) twinbuf.$(O) mysystem.$(O) keyfunc.$(O) \
	getline.$(O) getline2.$(O) keybound.$(O) dosshell.$(O) nnhash.$(O) \
	writer.$(O) history.$(O) ishell.$(O) scrshell.$(O) wildcard.$(O) cmdchdir.$(O) \
	shell.$(O) shell4.$(O) foreach.$(O) which.$(O) reader.$(O) nnvector.$(O) \
	ntcons.$(O) shellstr.$(O) cmds1.$(O) cmds2.$(O) xscript.$(O)  \
	strfork.$(O) lsf.$(O) open.$(O) nua.$(O) luautil.$(O)  \
	source.$(O) nnlua.$(O) dbcs.$(O)

ifeq ($(OS),Windows_NT)
OBJS=$(OBJS_) lua32com.$(O) win32com.$(O) getch_msvc.$(O) shortcut.$(O)
else
OBJS=$(OBJS_)
endif

ifeq ($(OS),Windows_NT)
nyaos.exe : $(OBJS) nyacusrc.$(O)
else
nyaos.exe : $(OBJS)
endif
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

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
nnlua.$(O) : nnlua.cpp nnlua.h
dbcs.$(O) : dbcs.cpp

# 環境依存ライブラリ
writer.$(O) : writer.cpp    writer.h
reader.$(O) : reader.cpp    reader.h
nndir.$(O) : nndir.cpp nndir.h
wildcard.$(O) : wildcard.cpp  nnstring.h nnvector.h nndir.h
ntcons.$(O) : ntcons.cpp
open.$(O) : open.cpp
getch_msvc.$(O) : getch_msvc.cpp
lua32com.$(O) : lua32com.cpp win32com.h
win32com.$(O) : win32com.cpp win32com.h

nua.$(O) : nua.cpp nua.h nnlua.h
luautil.$(O) : luautil.cpp

# リソース
nyacusrc.$(O)  : nyacus.rc luacat.ico
	windres --output-format=coff -o $@ $<

clean : 
	-$(RM) *.obj
	-$(RM) *.o
	-$(RM) *.exe
cleanobj :
	-$(RM) *.obj
	-$(RM) *.o
cleanorig :
	-$(RM) *.orig
	-$(RM) *.rej
cleanlua :
	$(MAKE) -C $(LUAPATH) clean

# vim:set noet ts=8 sw=8 nobk:

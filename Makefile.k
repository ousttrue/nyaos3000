# =============================================================
#   Makefile for NYAOS-II (InnoTek GCC 3.2.2+)
#   executable by GNU Make
# =============================================================
#
# make 
#	実行ファイルの作成
# make clean
#	*.obj,*.exe を削除
# make cleanobj
#	*.obj を削除
# This Makefile is for GNU Make.
#
# make package
#	*.zip の作成
# make install
#	$(INSTALLDIR) へ実行ファイルのコピー


NAME=NYAOS2

CP=cp -p
RM=rm -f
O=obj
X=.exe

# コンパイラ設定 
# -DHAVE_NOT_OLD_NEW_H <new.h> のかわりに <new> を include

# CCC=-DNDEBUG
CCC=-DNDEBUG
# GNU Make
CC=gcc
CXX=g++
#CFLAGS=-O2 -Zomf -Zsys -DOS2EMX $(CCC)
CFLAGS=-Wall -O3 -mcpu=pentium2 -march=pentium -Zomf -DOS2EMX -DHAVE_NOT_OLD_NEW_H $(CCC)
#CFLAGS=-Wall -O3 -mtune=pentium4 -march=pentium -Zomf -DOS2EMX -DHAVE_NOT_OLD_NEW_H $(CCC)
CXXFLAGS=$(CFLAGS)
LIBS=-lstdc++
LDFLAGS=-s -Zstack 512
RCBIND=rc

.SUFFIXES : $(X) .$(O) .res .rc .cpp .h

TARGET=$(NAME)$(X)

.cpp.$(O) :
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# make install した時に実行ファイルをコピーするディレクトリ
INSTALLDIR=\usr\bin\.

# ------------------------------------------------------------
OBJ1=nyados.$(O) nnstring.$(O) nndir.$(O) twinbuf.$(O) mysystem.$(O) keyfunc.$(O)
OBJ2=getline.$(O) getline2.$(O) keybound.$(O) dosshell.$(O) nnhash.$(O)
OBJ3=writer.$(O) history.$(O) ishell.$(O) scrshell.$(O) wildcard.$(O) cmdchdir.$(O)
OBJ4=shell.$(O) shell4.$(O) foreach.$(O) which.$(O) reader.$(O) nnvector.$(O)
OBJ5=ntcons.$(O) shellstr.$(O) cmds1.$(O) cmds2.$(O) xscript.$(O) shortcut.$(O)
OBJ6=strfork.$(O) lsf.$(O) open.$(O)
OBJS=$(OBJ1) $(OBJ2) $(OBJ3) $(OBJ4) $(OBJ5) $(OBJ6)

$(TARGET) : $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS) $(LIBS)
	$(RCBIND) nyaos2.rc $(TARGET)

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
cmdchdir.$(O)  : cmdchdir.cpp  shell.h nnstring.h nndir.h
foreach.$(O)   : foreach.cpp   shell.h
lsf.$(O) : lsf.cpp

# 共通ライブラリ
nnstring.$(O)  : nnstring.cpp  nnstring.h  nnobject.h
nnvector.$(O)  : nnvector.cpp  nnvector.h  nnobject.h
nnhash.$(O) : nnhash.cpp nnhash.h nnobject.h
strfork.$(O) : strfork.cpp

# 環境依存ライブラリ
writer.$(O)    : writer.cpp    writer.h
reader.$(O)    : reader.cpp    reader.h
nndir.$(O)       : nndir.cpp nndir.h
wildcard.$(O)  : wildcard.cpp  nnstring.h nnvector.h nndir.h
ntcons.$(O) : ntcons.cpp
open.$(O)  : open.cpp

install :
	$(CP) $(TARGET) $(INSTALLDIR)

package : $(NAME).txt $(TARGET)
	$(RM) *.$(O)
	zip $(NAME).zip $(NAME).txt $(TARGET) _$(NAME)
	zip nyasrc.zip Makefile* *.cpp *.h *.tmpl

clean : cleanobj
	-$(RM) $(TARGET)

cleanobj : 
	-$(RM) $(OBJS) core

todo :
	ruby todo.rb todo.txt > todo.html


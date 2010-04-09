#include "config.h"

#if defined(NYADOS)
#  include <dos.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef NYADOS
#  include <fcntl.h>
#  include <io.h>
#endif

#if defined(OS2EMX)
#  define INCL_DOSFILEMGR
#  include <os2.h>
#else
#  include <dir.h>
#endif

#include "nndir.h"

#if defined(__DMC__) && !defined(__OS2__)
#    undef FP_SEG
#    define FP_SEG(x) ((unsigned short)(((unsigned long)(x))>>16))
#    undef FP_OFF
#    define FP_OFF(x) ((unsigned short)(x))
#endif

/* Default Special Folder 設定のため */
#if defined(NYACUS)
#    include <windows.h>
#    include <shlobj.h>
#    include <stdio.h>
#endif

int NnTimeStamp::compare( const NnTimeStamp &o ) const
{
    return year   != o.year   ? o.year    - year 
	:  month  != o.month  ? o.month   - month
	:  day    != o.day    ? o.day     - day
	:  hour   != o.hour   ? o.hour    - hour
	:  minute != o.minute ? o.minute  - minute
	                      :  o.second - second ;
}

#ifdef _MSC_VER
static void stamp_conv( const FILETIME *p , NnTimeStamp &stamp_)
{
    FILETIME local;
    SYSTEMTIME s;

    FileTimeToLocalFileTime( p , &local );
    FileTimeToSystemTime( &local , &s );
    stamp_.second = s.wSecond ;
    stamp_.minute = s.wMinute ;
    stamp_.hour   = s.wHour ;
    stamp_.day    = s.wDay ;
    stamp_.month  = s.wMonth ;
    stamp_.year   = s.wYear ;
}
#else

static void stamp_conv( unsigned fdate , unsigned ftime , NnTimeStamp &stamp_ )
{
    /* 時刻 */
    stamp_.second = ( ftime & 0x1F) * 2 ;      /* 秒:5bit(0..31) */
    stamp_.minute = ( (ftime >> 5 ) & 0x3F );  /* 分:6bit(0..63) */
    stamp_.hour   =  ftime >> 11;              /* 時:5bit */

    /* 日付 */
    stamp_.day    = (fdate & 0x1F);            /* 日:5bit(0..31) */
    stamp_.month = ( (fdate >> 5 ) & 0x0F );   /* 月:4bit(0..16) */
    stamp_.year   =  (fdate >> 9 ) + 1980;     /* 年:7bit */
}
#endif

static void stamp_conv( time_t time1 , NnTimeStamp &stamp_ )
{
    struct tm *tm1;

    tm1 = localtime( &time1 );
    stamp_.second = tm1->tm_sec  ;
    stamp_.minute = tm1->tm_min  ;
    stamp_.hour   = tm1->tm_hour ;
    stamp_.day    = tm1->tm_mday ;
    stamp_.month  = tm1->tm_mon + 1;
    stamp_.year   = tm1->tm_year + 1900 ;
}

enum{
    W95_DATE_FORMAT = 0 ,
    DOS_DATE_FORMAT = 1 ,
};
extern int read_shortcut(const char *src,char *buffer,int size);

/** 「...\」→「..\..\」 DOS,OS/2 の為 */
void NnDir::extractDots( const char *&sp , NnString &dst )
{
    ++sp;
    for(;;){
	dst << "..";
	if( *++sp != '.' )
	    break;
	dst << '\\';
    }
}


NnDir::~NnDir()
{
    NnDir::findclose();
}
void NnDir::operator++()
{
    status = NnDir::findnext();
}
/* ファイルがあれば、ファイル名文字列オブジェクトへのポインタを返す。
 */
NnObject *NnDir::operator * ()
{
    return status ? 0 : &name_;
}

#ifdef NYADOS
/* ドライブが LFN をサポートしているかを調査をする。
 * return
 *	非0 : サポートしている.
 *      0 : サポートしていない.
 */
int isLfnOk()
{
    static int  result=-1;
    static char	rootdir[] = "?:\\";
    static char filesys[10];

    union REGS in, out;
#ifdef USE_FAR_PTR
    struct SREGS segs;
#endif

    if( result != -1 )
	return result;

    /* カレントドライブを取得する。*/
#ifdef ASM_OK
    _asm {
	mov ah,19h
	int 21h
	add ah,'a'
	mov rootdir[0],ah
    }
#else
    in.h.ah = 0x19;
    intdos(&in, &out);
    rootdir[0] = 'a' + out.h.al ;
#endif

    /* ドライブ情報を得る */
    in.x.ax   = 0x71A0;
    segs.ds   = FP_SEG(rootdir);
    segs.es   = FP_SEG(filesys);
    in.x.si   = FP_OFF(rootdir);
    in.x.di   = FP_OFF(filesys);
    in.x.cx   = sizeof(filesys);
    intdosx(&in,&out,&segs);

    return result=(out.x.ax != 0x7100 ? 1 : 0);
}

/* findfirst/findnext を呼ぶ際の共通部分.
 *	in -> 入力レジスタ情報
 *     out <- 出力レジスタ情報
 * return
 *	0 ... 成功
 *	1 ... 失敗(最後のファイルだった等)
 *	-1 .. 失敗(LFNがサポートされていない)
 */
#ifdef USE_FAR_PTR
unsigned NnDir::findcore( union REGS &in , union REGS &out , struct SREGS &segs )
#else
unsigned NnDir::findcore( union REGS &in , union REGS &out )
#endif
{
    struct { 
	unsigned long	attrib;
	unsigned short  ctime , cdate , dummy1 , dummy2 ;
	unsigned short  atime , adate , dummy3 , dummy4 ;
	unsigned short  mtime , mdate , dummy5 , dummy6 ;
	unsigned long	hsize, lsize;
	char	 	reserved[8];
	char	 	lname[260], name[14];
    } findbuf;

    memset( &findbuf , '\0' , sizeof(findbuf) );

#ifdef USE_FAR_PTR
    segs.es = FP_SEG( &findbuf );
#endif
    in.x.di = FP_OFF( &findbuf );

#ifdef USE_FAR_PTR
    unsigned result = intdosx(&in,&out,&segs);
#else
    unsigned result = intdos(&in,&out);
#endif
    if( result==0x7100 && isLfnOk()==0 )
	return -1;// AXに変化がなく、LFNがサポートされていない場合、エラー.

    if( out.x.cflag )
	return 1;// キャリーフラグが立っていた場合、終了マークと見なす。
    
    name_ = findbuf.lname ;
    attr_ = (unsigned)findbuf.attrib;
    size_ = (filesize_t)(findbuf.hsize << 32) | findbuf.lsize;
    stamp_conv( findbuf.mdate,  findbuf.mtime , stamp_ );
    
    return 0;
}
#endif

/* Short FN しか使えない場合のファイルバッファ:固定 */
#if defined(OS2EMX)
    static _FILEFINDBUF4 findbuf;
    static ULONG findcount;
#elif defined(__DMC__)
    static struct find_t findbuf;
#elif defined(_MSC_VER)
	#include <windows.h>
	static WIN32_FIND_DATA wfd;
	static HANDLE hfind = INVALID_HANDLE_VALUE;
	static DWORD attributes;
#else
    static struct ffblk findbuf;
#endif

#ifdef _MSC_VER
static int findfirst( const char* path, DWORD attr)
{
    attributes = attr;
    hfind = ::FindFirstFile( path, &wfd);
    if( hfind==INVALID_HANDLE_VALUE){
	return -1;
    }
    return 0;
}
static int findnext()
{
    if( ::FindNextFile( hfind, &wfd)==FALSE){
	return -1;
    }
    return 0;
}
static int findclose()
{
    return ::FindClose( hfind );
}
#endif

/* いわゆる _dos_findfirst
 *	p_path - パス (ワイルドカード部含む)
 *	attr   - アトリビュート
 * return
 *	0 ... 成功
 *	1 ... 失敗(最後のファイルだった等)
 *	-1 .. 失敗(LFNがサポートされていない)
 */
unsigned NnDir::findfirst(  const char *path , unsigned attr )
{
    NnString path2(path);
    return NnDir::findfirst( path2 , attr );
}

unsigned NnDir::findfirst(  const NnString &p_path , unsigned attr )
{
    unsigned result;
    NnString path;
    filter( p_path.chars() , path );

#ifdef NYADOS
    union REGS in,out;
#ifdef USE_FAR_PTR
    struct SREGS segs;
#endif
    if( isLfnOk() ){
	in.h.cl = attr;
#ifdef USE_FAR_PTR
        segs.ds = FP_SEG(path.chars());
#endif
	in.x.dx = FP_OFF(path.chars());
	in.h.ch = 0 ;
	in.x.ax = 0x714E;
	in.x.si = DOS_DATE_FORMAT;

#ifdef USE_FAR_PTR
	if( (result = findcore(in,out ,segs))==0 ){
#else
	if( (result = findcore(in,out))==0 ){
#endif
	    handle = out.x.ax;
	    hasHandle = 1;
	}else{
	    hasHandle = 0;
	}
	return result;
    }
#endif
    hasHandle = 0;
    handle = 0;
#if defined(OS2EMX)
    /***  emx/gcc code for NYAOS-II ***/
    handle = 0xFFFFFFFF;
    findcount = 1 ;
    result = DosFindFirst(    (PUCHAR)path.chars() 
			    , (PULONG)&handle 
			    , attr 
			    , &findbuf
			    , sizeof(_FILEFINDBUF4) 
			    , &findcount 
			    , (ULONG)FIL_QUERYEASIZE );
    if( result == 0 ){
        name_ = findbuf.achName ;
	attr_ = findbuf.attrFile ;
	size_ = findbuf.cbFile ;
	stamp_conv( *(unsigned short*)&findbuf.fdateLastWrite ,
		    *(unsigned short*)&findbuf.ftimeLastWrite , stamp_ );
	hasHandle = 1;
    }
#elif defined(__DMC__) 
#if defined(__OS2__)
    FIND *findbuf=::findfirst( path.chars() , attr );
    if( findbuf != NULL ){
        name_ = findbuf->name;
        attr_ = findbuf->attribute;
    }
#else
    /*** Digitalmars C++ for NYADOS ***/
    result = ::_dos_findfirst( path.chars() , attr , &findbuf );
    if( result == 0 ){
        name_ = findbuf.name ;
	attr_ = findbuf.attrib;
	stamp_conv( findbuf.wr_date , findbuf.wr_time , stamp_ );
    }
#endif
#elif defined(_MSC_VER)
    /**** VC++ code for NYACUS but not maintenanced by hayama ****/
    result = ::findfirst( path.chars(), attr);
    if( result==0){
	name_ = wfd.cFileName;
	attr_ = wfd.dwFileAttributes;
        size_ = (((filesize_t)wfd.nFileSizeHigh << 32) | wfd.nFileSizeLow) ;
        stamp_conv( &wfd.ftLastWriteTime , stamp_ );
	hasHandle = 1;
    }
#else /*** Borland-C++ code for NYACUS ***/
    result = ::findfirst( path.chars() , &findbuf , (int)attr );
    if( result == 0 ){
	name_ = findbuf.ff_name;
	attr_ = findbuf.ff_attrib;
	size_ = findbuf.ff_fsize;
	stamp_conv( findbuf.ff_fdate , findbuf.ff_ftime , stamp_ );

	hasHandle = 1;
    }
#endif
    return result;
}

unsigned NnDir::findnext()
{
#ifdef NYADOS
    if( isLfnOk() ){
	union REGS in,out;
#ifdef USE_FAR_PTR
        struct SREGS segs;
#endif
	in.x.ax = 0x714F;
	in.x.bx = handle;
	in.x.si = DOS_DATE_FORMAT;
#ifdef USE_FAR_PTR
	return findcore(in,out,segs);
#else
	return findcore(in,out);
#endif
    }
#endif
#if defined(OS2EMX)
    /*** emx/gcc code for NYAOS-II(OS/2) ***/
    int result=DosFindNext(handle,&findbuf,sizeof(findbuf),&findcount);
    if( result == 0 ){
	name_ = findbuf.achName;
	attr_ = findbuf.attrFile;
	size_ = findbuf.cbFile ;
	stamp_conv( *(unsigned short*)&findbuf.fdateLastWrite ,
		    *(unsigned short*)&findbuf.ftimeLastWrite , stamp_ );
    }
#elif defined(__DMC__)
#  if defined(__OS2__)
    /*** DigitalMars C++ for NYAOS-II ***/
    int result=-1;
    struct FIND *entry=::findnext();
    if( entry != NULL ){
        result = 0;
        name_ = entry->name;
        attr_ = entry->attribute;
    }
#else
    /*** DigitalMars C++ for NYADOS ***/
    int result=::_dos_findnext( &findbuf );
    if( result == 0 ){
	name_ = findbuf.name;
	attr_ = findbuf.attrib;
	stamp_conv( findbuf.wr_date , findbuf.wr_time , stamp_ );
    }
#endif
#elif defined(_MSC_VER)
    /*** Visual C++ ***/
    int result = ::findnext();
    if( result==0){
	name_ = wfd.cFileName;
	attr_ = wfd.dwFileAttributes;
        size_ = (((filesize_t)wfd.nFileSizeHigh << 32) | wfd.nFileSizeLow) ;
        stamp_conv( &wfd.ftLastWriteTime , stamp_ );
    }
#else /*** Borland-C++ for NYACUS ***/
    int result=::findnext( &findbuf );
    if( result == 0 ){
	name_ = findbuf.ff_name ;
	attr_ = findbuf.ff_attrib;
	size_ = findbuf.ff_fsize;
	stamp_conv( findbuf.ff_fdate , findbuf.ff_ftime , stamp_ );

    }
#endif
    return result;
}

void NnDir::findclose()
{
#ifdef NYADOS
    union	REGS	in, out;
#endif

    if( hasHandle ){
#if defined(NYADOS)
	in.x.ax = 0x71A1;
	in.x.bx = handle;
	intdos(&in, &out);
#elif defined(OS2EMX)
        DosFindClose( handle );
#elif defined(_MSC_VER)
	::findclose();
#elif !defined(__DMC__)
	::findclose( &findbuf );
#endif
	hasHandle = 0;
    }
}

/* カレントディレクトリを得る.(LFN対応の場合のみ)
 *	pwd - カレントディレクトリ
 * return
 *	0 ... 成功
 *	1 ... 失敗
 */
int NnDir::getcwd( NnString &pwd )
{
#ifdef NYADOS
    if( ! isLfnOk() ){
#endif
	char buffer[256];
#ifdef OS2EMX
	if( ::_getcwd2(buffer,sizeof(buffer)) != NULL ){
#else
	if( ::getcwd(buffer,sizeof(buffer)) != NULL ){
#endif
	    pwd = buffer;
	}else{
            pwd.erase();
        }
	return 0;
#ifdef NYADOS
    }
    union  REGS	 in, out;
    static char	tmpcur[] = ".";
    char   tmpbuf[256];

    in.x.cx = 0x8002;
    in.x.ax = 0x7160;
    in.x.si = FP_OFF(tmpcur);
    in.x.di = FP_OFF(tmpbuf);

#ifdef USE_FAR_PTR
    struct SREGS segs;
    segs.ds = FP_SEG(tmpcur);
    segs.es = FP_SEG(tmpbuf);
    intdosx(&in,&out,&segs);
#else
    intdos(&in, &out );
#endif

    // エラーの場合、何もせず、終了.
    if( out.x.cflag ){
        pwd.erase();
	return 1;
    }
    pwd = tmpbuf;
    return 0;
#endif
}

/* スラッシュをバックスラッシュへ変換する
 * 重複した \ や / を一つにする.
 * 	src - 元文字列
 *	dst - 結果文字列の入れ物
 */
void NnDir::f2b( const char *sp , NnString &dst )
{
    int prevchar=0;
    while( *sp != '\0' ){
	if( isKanji(*sp & 255) ){
	    prevchar = *sp;
	    dst << *sp++;
	    if( *sp == 0 )
		break;
	    dst << *sp++;
	}else{
	    if( *sp == '/' || *sp == '\\' ){
		if( prevchar != '\\' )
		    dst << '\\';
		prevchar = '\\';
		++sp;
	    }else{
		prevchar = *sp;
		dst << *sp++;
	    }
	}
    }
    // dst = sp; dst.slash2yen();
}


/* パス名変換を行う。
 * ・スラッシュをバックスラッシュへ変換
 * ・チルダを環境変数HOMEの内容へ変換
 * 	src - 元文字列
 *	dst - 結果文字列の入れ物
 */
void NnDir::filter( const char *sp , NnString &dst_ )
{
    NnString dst;

    dst.erase();
    const char *home;
    if( *sp == '~'  && (isalnum( *(sp+1) & 255) || *(sp+1)=='_') ){
	NnString name;
	++sp;
	do{
	    name << *sp++;
	}while( isalnum(*sp & 255) || *sp == '_' );
	NnString *value=(NnString*)specialFolder.get(name);
	if( value != NULL ){
	    dst << *value;
	}else{
	    dst << '~' << name;
	}
    }else if( *sp == '~'  && 
             ( (home=getEnv("HOME",NULL))        != NULL  ||
               (home=getEnv("USERPROFILE",NULL)) != NULL ) )
    {
	dst << home;
	++sp;
    }else if( sp[0]=='.' &&  sp[1]=='.' && sp[2]=='.' ){
	extractDots( sp , dst );
    }
    dst << sp;

    f2b( dst.chars() , dst_ );
    // dst.slash2yen();
}

/* LFN対応 chdir.
 *	argv - ディレクトリ名
 * return
 *	 0 - 成功
 *	-1 - 失敗
 */
int NnDir::chdir( const char *argv )
{
    NnString newdir;
    filter( argv , newdir );
#ifdef NYADOS
    if( isLfnOk() ){
	union REGS in,out;
	if( isAlpha(newdir.at(0)) && newdir.at(1)==':' ){
	    in.h.ah = 0x0E;
	    in.h.dl = (newdir.at(0) & 0x1F ) - 1;
	    intdos( &in , &out );
	}
	in.x.ax = 0x713B;
	in.x.dx = FP_OFF( newdir.chars() );
#ifdef USE_FAR_PTR
        struct SREGS segs;
        segs.ds = FP_SEG( newdir.chars() );
	intdosx(&in,&out,&segs);
#else
	intdos(&in,&out);
#endif
	return out.x.cflag ? -1 : 0;
    }
#endif
    if( isAlpha(newdir.at(0)) && newdir.at(1)==':' ){
#ifdef OS2EMX
        _chdrive( newdir.at(0) );
#else
        setdisk( (newdir.at(0) & 0x1F)-1 );
#endif
    }
    if(    newdir.at( newdir.length()-1 ) == '\\'
        || newdir.at( newdir.length()-1 ) == '/' )
        newdir += '.';

    if( newdir.iendsWith(".lnk") ){
	char buffer[ FILENAME_MAX ];
	if( read_shortcut( newdir.chars() , buffer , sizeof(buffer) )==0 ){
	    newdir = buffer;
	}
    }
    return ::chdir( newdir.chars() );
}

/* ファイルクローズ
 * (特にLFN対応というわけではないが、コンパイラ依存コードを
 *  避けるため作成)
 *      fd - ハンドル
 */
void NnDir::close( int fd )
{
#ifdef NYADOS
    union REGS in,out;

    in.h.ah = 0x3e;
    in.x.bx = fd;
    intdos(&in,&out);
#else
    ::close(fd);
#endif
}
/* ファイル出力
 * (特にLFN対応というわけではないが、コンパイラ依存コードを
 *  避けるため作成)
 *      fd - ハンドル
 *      ptr - バッファのアドレス
 *      size - バッファのサイズ
 * return
 *      実出力サイズ (-1 : エラー)
 */
int NnDir::write( int fd , const void *ptr , size_t size )
{
#ifdef NYADOS
    union REGS in,out;
    struct SREGS sregs;

    in.h.ah  = 0x40;
    in.x.bx  = fd;
    in.x.cx  = size;
    in.x.dx  = FP_OFF(ptr);
    sregs.ds = FP_SEG(ptr);
    intdosx(&in,&out,&sregs);
    return out.x.cflag ? -1 : out.x.ax;
#else
    return ::write(fd,ptr,size);
#endif
}

#ifdef NYADOS
/* パス名をショートファイル名へ変換する。
 *     src - ロングファイル名
 * return
 *     dst - ショートファイル名(static領域)
 */
const char *NnDir::long2short( const char *src )
{
    static char dst[ 67 ];
    
    dst[0] = '\0';

    union REGS in,out;
    struct SREGS sregs;

    in.x.ax = 0x7160;
    in.h.cl = 0x01;
    in.h.ch = 0x80;

    sregs.ds = FP_SEG(src);
    in.x.si  = FP_OFF(src);

    sregs.es = FP_SEG(dst);
    in.x.di  = FP_OFF(dst);
    intdosx(&in,&out,&sregs);
    if( out.x.cflag || dst[0] == '\0' )
	return src;
    
    return dst;
}


#endif

#ifdef NYADOS
/* ファイルの書きこみ位置を末尾へ移動させる. */
int NnDir::seekEnd( int handle )
{
    union REGS in,out;

    // アペンドの場合、ファイルの書きこみ位置を末尾に移動させる.
    in.x.ax = 0x4202;
    in.x.bx = handle;
    in.x.cx = 0;
    in.x.dx = 0;
    intdos(&in,&out);

    return out.x.cflag ? -1 : 0 ;
}
#endif

/* LFN 対応 OPEN
 *      fname - ファイル名
 *      mode - "w","r","a" のいずれか
 * return
 *      ハンドル(エラー時:-1)
 */
int NnDir::open( const char *fname , const char *mode )
{
#ifdef NYADOS
    if( mode == NULL )
        return -1;
    
    union REGS in,out;
    struct SREGS sregs;

    in.x.ax  = ( isLfnOk() ? 0x716C : 0x6C00 );

    // BXの値を間違えると、オープンは出来るが,
    // １バイトも書きこめないので注意が必要.
    if( *mode == 'r' ){
        in.x.bx = 0x2000;
        in.x.dx = 0x01;
    }else{
        in.x.bx = 0x2001;                    // アクセス/シェアリングモード.
        in.x.dx  = ( *mode == 'a' ? 0x11 : 0x12 );  // 動作フラグ.
    }
    in.x.cx  = 0x0000;                    // 属性.
    in.x.si  = FP_OFF( fname );           // ファイル名.
    sregs.ds = FP_SEG( fname );           // ファイル名.
    intdosx( &in , &out , &sregs );

    if( out.x.cflag )
        return -1;

    int fd=out.x.ax;

    if( *mode == 'a' ){
	if( NnDir::seekEnd( fd ) != 0 ){
            NnDir::close(fd);
            return -1;
        }
    }
    return fd;
#else
    switch( *mode ){
    case 'w':
        return ::open(fname ,
                O_WRONLY|O_BINARY|O_CREAT|O_TRUNC , S_IWRITE|S_IREAD );
    case 'a':
        {
            int fd= ::open(fname ,
                O_WRONLY|O_BINARY|O_CREAT|O_APPEND , S_IWRITE|S_IREAD );
            lseek(fd,0,SEEK_END);
            return fd;
        }
    case 'r':
        return ::open(fname , O_RDONLY|O_BINARY , S_IWRITE|S_IREAD );
    default:
        return -1;
    }
#endif
}

/* テンポラリファイル名を作る.  */
const NnString &NnDir::tempfn()
{
    static NnString result;

    /* ファイル名部分(非ディレクトリ部分)を作る */
    char tempname[ FILENAME_MAX ];
    tmpnam( tempname );
    
    /* ディレクトリ部分を作成 */
    const char *tmpdir;
    if(   ((tmpdir=getEnv("TEMP",NULL)) != NULL )
       || ((tmpdir=getEnv("TMP",NULL))  != NULL ) ){
        result  = tmpdir;
        result += '\\';
        result += tempname;
    }else{
        result  = tempname;
    }
    return result;
}

int NnDir::access( const char *path )
{
    NnDir dir(path); return *dir == NULL;
}

/* カレントドライブを変更する
 *    driveletter - ドライブ文字('A'..'Z')
 * return 0:成功,!0:失敗
 */
int  NnDir::chdrive( int driveletter )
{
    return 
#ifdef OS2EMX
	_chdrive( driveletter );
#else
	setdisk( (driveletter & 0x1F )- 1 );
#endif
}

/* カレントドライブを取得する。
 * return ドライブ文字 'A'...'Z'
 */
int NnDir::getcwdrive()
{
    return 
#ifdef OS2EMX
	_getdrive();
#elif defined(__TURBOC__)
	'A'+getdisk();
#else
	'A'+ _getdrive() - 1;
#endif
}

int NnFileStat::compare( const NnSortable &another ) const
{
    return name_.icompare( ((NnFileStat&)another).name_ );
}
NnFileStat::~NnFileStat(){}

#ifdef NYACUS
/* My Document などを ~desktop などと登録する
 * 参考⇒http://www001.upp.so-net.ne.jp/yamashita/doc/shellfolder.htm
 */
void NnDir::set_default_special_folder()
{
    static struct list_s {
	const char *name ;
	int  key;
    } list[]={
	{ "desktop"           , CSIDL_DESKTOPDIRECTORY },
	{ "sendto"            , CSIDL_SENDTO },
	{ "startmenu"         , CSIDL_STARTMENU },
	{ "startup"           , CSIDL_STARTUP },
	{ "mydocuments"       , CSIDL_PERSONAL },
	{ "favorites"         , CSIDL_FAVORITES },
	{ "programs"          , CSIDL_PROGRAMS },
	{ "program_files"     , CSIDL_PROGRAM_FILES },
	{ "appdata"           , CSIDL_APPDATA },
	{ "allusersdesktop"   , CSIDL_COMMON_DESKTOPDIRECTORY },
	{ "allusersprograms"  , CSIDL_COMMON_PROGRAMS },
	{ "allusersstartmenu" , CSIDL_COMMON_STARTMENU },
	{ "allusersstartup"   , CSIDL_COMMON_STARTUP },
	{ NULL , 0 },
    };
    LPITEMIDLIST pidl;
    char path[ FILENAME_MAX ];

    for( struct list_s *p=list ; p->name != NULL ; p++ ){
	path[0] = '\0';

	::SHGetSpecialFolderLocation( NULL , p->key, &pidl );
	::SHGetPathFromIDList( pidl, path );

	if( path[0] != '\0' )
	    specialFolder.put( p->name , new NnString( path ) );
    }
}

#else
void NnDir::set_default_special_folder()
{
    /* do nothing */
}
#endif

NnHash NnDir::specialFolder;

NnFileStat *NnFileStat::stat(const NnString &name)
{
    NnTimeStamp stamp1;
#ifdef _MSC_VER
    struct _stati64 stat1;
#else
    struct stat stat1;
#endif
    unsigned attr=0 ;
#ifdef __DMC__
    NnString name_( NnDir::long2short(name.chars()) );
#else
    NnString name_(name);
#endif
    if( name_.endsWith(":") || name_.endsWith("\\") || name_.endsWith("/") )
	name_ << ".";

#ifdef _MSC_VER
    if( ::_stati64( name_.chars() , &stat1 ) != 0 )
#else
    if( ::stat( name_.chars() , &stat1 ) != 0 )
#endif
    {
	return NULL;
    }
    if( stat1.st_mode & S_IFDIR )
	attr |= ATTR_DIRECTORY ;
#ifdef __DMC__
    if( (stat1.st_mode & S_IWRITE) == 0 )
#else
    if( (stat1.st_mode & S_IWUSR) == 0 )
#endif
	attr |= ATTR_READONLY ;
    
    stamp_conv( stat1.st_mtime , stamp1 );

    return new NnFileStat(
	name ,
	attr ,
	stat1.st_size ,
	stamp1 );
}

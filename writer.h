#ifndef WRITER_H
#define WRITER_H
#include "nnstring.h"
#include <stdio.h>

class Writer : public NnObject {
public:
    virtual ~Writer();
    virtual Writer &write( char c )=0;
    virtual Writer &write( const char *s)=0;
    virtual int isatty() const;

    Writer &operator << ( char c ){ return write(c); }
    Writer &operator << ( const char *s ){ return write(s); }
    Writer &operator << ( int n );
    Writer &operator << (const NnString &s){ return *this << s.chars(); }
};

/* <stdio.h> の FILE* を通じて出力する Writer系クラス.
 * ファイルのオープンクローズなどはしない。
 * ほとんど、stdout,stderr専用
 */
class StreamWriter : public Writer {
    FILE *fp_;
protected:
    FILE *fp(){ return fp_; }
    void setFp( FILE *fp ){ fp_ = fp; }
public:
    ~StreamWriter(){}
    StreamWriter() : fp_(NULL) {}
    StreamWriter( FILE *fp ) : fp_(fp){}
    Writer &write( char c );
    Writer &write( const char *s );
    int ok() const { return fp_ != NULL ; }
    int fd() const { return fileno(fp_); }
    int isatty() const;
};

/* テキストファイルに出力する Writer系クラス。
 * 終了時にファイルをクローズする。
 */
#ifdef NYADOS
    /* NYADOS では,VFAT-API & RAW I/O API を使った実装を行う */
    class FileWriter : public Writer {
        unsigned short fd_,size;
        char buffer[128];
        void put(char c);
    public:
        ~FileWriter();
        FileWriter( const char *fn     , const char *mode );
        FileWriter( const NnString &fn , const char *mode );
        Writer &write( char c );
        Writer &write( const char *s );
        int ok(){ return fd_ != -1; }
        int fd(){ return fd_; }
	int (isatty)() const ;
    };
#else
    /* NYADOS以外では、普通に FILE* を使った実装を使う */
    class FileWriter : public StreamWriter {
    public:
        ~FileWriter();
        FileWriter( const char *fn , const char *mode );
    };
#endif

class PipeWriter : public StreamWriter {
#ifdef NYADOS
    NnString tempfn;
#endif
    NnString cmds;
    void open( const NnString &cmds );
public:
    ~PipeWriter();
    PipeWriter( const char *cmds );
    PipeWriter( const NnString &cmds ){ open(cmds); }
};

#ifdef NYACUS

/* エスケープシーケンスを解釈して、画面のアトリビュートの
 * コントロールまで行う出力クラス.
 */
class AnsiConsoleWriter : public StreamWriter {
    static int default_color;
    int flag;
    int param[20];
    size_t n;
    enum { PRINTING , STACKING } mode ;
    enum { BAREN = 1 , GREATER = 2 } ;
public:
    AnsiConsoleWriter( FILE *fp ) 
	: StreamWriter(fp) , flag(0) , n(0) , mode(PRINTING) {}

    Writer &write( char c );
    Writer &write( const char *p );
};

#else

#define AnsiConsoleWriter StreamWriter

#endif

extern AnsiConsoleWriter conOut,conErr;

/* Writer のポインタ変数に対してリダイレクトする Writer クラス. */
class WriterClone : public Writer {
    Writer **rep;
public:
    WriterClone( Writer **rep_ ) : rep(rep_) {}
    ~WriterClone(){}
    Writer &write( char c ){ return (*rep)->write( c ); }
    Writer &write( const char *s ){ return (*rep)->write( s ); }
};

/* 出力内容を全て捨ててしまう Writer */
class NullWriter : public Writer {
public:
    NullWriter(){}
    ~NullWriter(){}
    Writer &write( char c ){ return *this; }
    Writer &write( const char *s ){ return *this; }
};



/* 標準出力・入力をリダイレクトしたり、元に戻したりするクラス */
class Redirect {
    int original_fd;
    int fd_;
    int isOpenFlag;
public:
    Redirect(int x) : original_fd(-1) , fd_(x) {}
    ~Redirect(){ reset(); }
    void close();
    void reset();
    void set(int x);
    int  fd() const { return fd_; }

    int switchTo( const NnString &fn , const char *mode );
};


#endif

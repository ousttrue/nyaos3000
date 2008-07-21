#ifndef READER_H
#define READER_H

#include <stdio.h>
#include "nndir.h"

class Reader : public NnObject {
public:
    virtual int getchr()=0;
    virtual int eof() const =0 ;
    int readLine( NnString &line );
};

class StreamReader : public Reader {
protected:
    FILE *fp;
public:
    StreamReader() : fp(0){}
    StreamReader(FILE *fp_) : fp(fp_){}
    virtual int getchr();
    virtual int eof() const;
    int fd() const { return fileno(fp); }
};

class FileReader : public StreamReader {
public:
    FileReader(){}
    FileReader( const char *fn );
    ~FileReader();

    int open( const char *path );
    void close();
    void getpos( fpos_t &pos ){ fgetpos(fp,&pos); }
    void setpos( fpos_t &pos ){ fsetpos(fp,&pos); }
};

class PipeReader : public StreamReader {
#ifdef NYADOS
    NnString tempfn;
#endif
    void open( const char *cmds );
    void open_null();
public:
    void close();
    PipeReader( const char *cmds );
    PipeReader();
    ~PipeReader();
};

#endif

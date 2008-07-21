#include <stdlib.h>
#include "reader.h"

/* ˆês“ü—Í
 *    line - “ü—Í•¶Žš—ñ
 * return
 *     •¶Žš” / -1:EOF
 */
int Reader::readLine( NnString &line )
{
    line.erase();
    if( this->eof() )
	return -1;

    int ch;
    while( (ch=this->getchr()) != '\n' && ch != EOF )
	line += char(ch);
    
    return line.length();
}


int StreamReader::eof() const
{
    return fp==NULL || feof(fp);
}

int StreamReader::getchr()
{
    return fp==NULL || feof(fp) ? EOF : getc(fp);
}


FileReader::FileReader( const char *path )
{
    fp = NULL;
    this->open( path );
}

int FileReader::open( const char *path )
{
    this->close();
    fp = fopen( NnDir::long2short(path) , "r" );
    return fp != NULL ? 0 : -1 ;
}

void FileReader::close()
{
    if( fp != NULL )
	fclose( fp );
    fp = NULL;
}

FileReader::~FileReader()
{
    this->close();
}
PipeReader::~PipeReader()
{
    this->close();
}

void PipeReader::open( const char *cmds )
{
    this->close();
#ifdef NYADOS
    NnString cmd1(cmds);
    this->tempfn=NnDir::tempfn();

    cmd1 << " > " << tempfn;
    system( cmd1.chars() );
    fp = fopen( tempfn.chars() , "r" );
#elif defined(__BORLANDC__)
    fp = _popen( cmds , "r" );
#else
    fp = popen( cmds , "r" );
#endif
}

PipeReader::PipeReader( const char *cmds )
{
    fp = NULL;
    this->open( cmds );
}

void PipeReader::close()
{
    if( fp != NULL ){
#ifdef NYADOS
        fclose(fp);
#elif defined(__BORLANDC__)
	_pclose(fp);
#else
	pclose(fp);
#endif
        fp = NULL;
    }
#ifdef NYADOS
    if( tempfn.length() > 0 ){
        ::remove( tempfn.chars() );
        tempfn.erase();
    }
#endif
}


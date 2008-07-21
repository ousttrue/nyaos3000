#include "ishell.h"
#include "nnhash.h"

int History::get(int at,NnString &dst)
{
    NnString *src=(*this)[at];
    if( src == NULL )
        return -1;
    
    dst = *src;
    return 0;
}
int History::set(int at,NnString &str)
{ 
    NnString *dst=(*this)[at];
    if( dst == NULL )
        return -1;
    
    *dst = str;
    return 0;
}

int InteractiveShell::readline( NnString &line )
{
    NnObject *val;

    if( nesting.size() > 0 ){
        dosShell.setPrompt( *(NnString*)nesting.top() );
    }else if( (val=properties.get("prompt")) != NULL ){
	dosShell.setPrompt( *(NnString*)val );
    }else{
        NnString prompt( getEnv("PROMPT") );
        dosShell.setPrompt( prompt );
    }
    // if( (val=properties.get("title1")) != NULL ){
    //	  Console::setTitle( ((NnString*)val)->chars() );
    // }
    return dosShell(line);
}

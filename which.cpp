#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include "nndir.h"
#include "nnstring.h"
#include "shell.h"

/* 実行ファイルのパスを探す
 *      nm      < 実行ファイルの名前
 *      which   > 見つかった場所
 * return
 *      0 - 発見
 *      -1 - 見つからず
 */
int which( const char *nm, NnString &which )
{
    NnString rest(".");
    const char *env=getEnv("PATH",NULL);
    if( env != NULL )
	rest << ";" << env ;
    
    while( ! rest.empty() ){
	NnString path;
	rest.splitTo( path , rest , ";" );
	if( path.empty() )
	    continue;
        path << '\\' << nm;
        if( NnDir::access(path.chars())==0 ){
            which = path;
            return 0;
        }
    }
    return -1;
}

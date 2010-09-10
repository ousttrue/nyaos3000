#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include "nndir.h"
#include "nnstring.h"
#include "shell.h"

/* 拡張子を持つファイル名であれば 1 を返す */
static int has_dot(const char *path)
{
    int lastdot=NnString::findLastOf(path,"/\\.");
    if( lastdot >= 0 && path[ lastdot ] == '.' )
        return 1;
    else
        return 0;
}

/* フルパス一歩手前になっているファイル名に、.exe 拡張子を補って、
 * 存在があるかを確認する
 *    nm - フルパス一歩手前になっているファイル名
 *    has_dot_f - nm の中に「.」が含まれていたら真をセットする
 *    which - 見付かった時に、補ったパス名を格納する先
 * return
 *     0 - 見付かった(which に値が入る)
 *    -1 - 見付からなかった
 *    -2 - 見付かったが、実行拡張子を持たない
 */
static int exists( const char *nm , int has_dot_f , NnString &which )
{
    static const char *suffix_list[]={
        ".exe" , ".com" , ".bat" , ".cmd" , NULL 
    };
    NnString path(nm);

    for( const char **p=suffix_list ; *p != NULL ; ++p ){
        path << *p;
        if( NnDir::access(path.chars()) == 0 ){
            which = path ;
            return 0;
        }
        path.chop( path.length()-4 );
    }
    /* 拡張子の無いファイルは、たとえ存在しても実行できない */
    if( has_dot_f && NnDir::access(path.chars()) == 0 ){
        which = path ;
        return 0;
    }
    return -1;
}


/* 実行ファイルのパスを探す
 *      nm      < 実行ファイルの名前
 *      which   > 見つかった場所
 * return
 *      0 - 発見
 *      -1 - 見つからず
 */
int which( const char *nm, NnString &which )
{
    int has_dot_f = has_dot(nm);
    if( exists(nm,has_dot_f,which)==0 )
        return 0;

    /* 相対パス指定・絶対パス指定しているものは、
     * 環境変数PATHをたどってまで、検索しない
     */
    if( NnString::findLastOf(nm,"/\\") >= 0 ){
        return -1;
    }

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
        if( exists(path.chars(),has_dot_f,which)==0 )
            return 0;
    }
    return -1;
}

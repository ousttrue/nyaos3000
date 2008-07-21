#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <io.h>
#include <dos.h>
#include <string.h>

#include "config.h"

#ifndef NYADOS
#  include <fcntl.h>
#endif

#include "nnstring.h"
#include "nnvector.h"
#include "nnhash.h"
#include "writer.h"
#include "nndir.h"
#include "shell.h"

enum{
    TOO_LONG_COMMANDLINE = -3  ,
    MAX_COMMANDLINE_SIZE = 127 ,
};


/* 代替spawn。spawnのインターフェイスを NNライブラリに適した形で提供する。
 *      args - パラメータ
 *      wait - P_WAIT , P_NOWAIT
 * return
 *      spawn の戻り値
 */
static int mySpawn( const NnVector &args , int wait )
{
    const char **argv
        =(const char**)malloc( sizeof(const char *)*(args.size() + 1) );
    for(int i=0 ; i<args.size() ; i++)
        argv[i] = ((NnString*)args.const_at(i))->chars();
    argv[ args.size() ] = NULL;
    int result;
    result = spawnvp( wait , (char*)NnDir::long2short(argv[0]) , (char**)argv );
    free( argv );
    return result;
}

static void parseRedirect( const char *&cmdline , Redirect &redirect )
{
    const char *mode="w";
    if( *cmdline == '>' ){
	mode = "a";
	++cmdline;
    }
    NnString path;
    NyadosShell::readNextWord( cmdline , path );
    if( redirect.switchTo( path , mode ) != 0 ){
	conErr << path << ": can't open.\n";
    }
}

/* リダイレクトの > の右側移行の処理を行う.
 *	sp : > の次の位置
 *	redirect : リダイレクトオブジェクト
 */
static void after_lessthan( const char *&sp , Redirect &redirect )
{
    if( sp[0] == '&'  &&  isdigit(sp[1]) ){
	/* n>&m */
	redirect.set( sp[1]-'0' );
	sp += 2;
    }else if( sp[0] == '&' && sp[1] == '-' ){
	/* n>&- */
	redirect.switchTo( "nul", "w" );
    }else{
	/* n> ファイル名 */
	parseRedirect( sp , redirect );
    }
}

/* コマンドラインを | で分割して、ベクターに各コマンドを代入する
 *	cmdline - コマンドライン
 *	vector - 各要素が入る配列
 */
static void devidePipes( const char *cmdline , NnVector &vector )
{
    NnString one,left(cmdline);
    while( left.splitTo(one,left,"|") , !one.empty() )
	vector.append( one.clone() );
}

/* NYADOS 専用：リダイレクト処理付き１コマンド処理ルーチン
 *	cmdline - コマンド
 *	wait - P_WAIT or P_NOWAIT
 * return
 *	コマンドの実行結果
 */
static int do_one_command( const char *cmdline , int wait )
{
    Redirect redirect0(0);
    Redirect redirect1(1);
    Redirect redirect2(2);
    NnString execstr;

    int quote=0;
    while( *cmdline != '\0' ){
	if( *cmdline == '"' )
	    quote ^= 1;

        if( quote==0 && cmdline[0]=='<' ){
            ++cmdline;
            NnString path;
            NyadosShell::readNextWord( cmdline , path );
            if( redirect0.switchTo( path , "r" ) != 0 ){
                conErr << path << ": can't open.\n";
            }
        }else if( quote==0 && cmdline[0]=='>' ){
	    ++cmdline;
	    parseRedirect( cmdline , redirect1 );
	}else if( quote==0 && cmdline[0]=='1' && cmdline[1]=='>' ){
	    cmdline += 2;
	    after_lessthan( cmdline , redirect1 );
	}else if( quote==0 && cmdline[0]=='2' && cmdline[1]=='>' ){
	    cmdline += 2;
	    after_lessthan( cmdline , redirect2 );
	}else{
	    execstr << *cmdline++;
	}
    }
#ifdef NYADOS
    /* NYADOS 以外の場合は、ここに来る時は、既に
     * standalone モードであることが確定しているので、
     * 判断する必要がなくない。
     */
    if( properties.get("standalone") == NULL ){
	return system( execstr.chars() );
    }
#endif
    NnVector args;

    execstr.splitTo( args );
    return mySpawn( args , wait );
}

/* NYADOS 専用：system代替関数（パイプライン処理）
 *	cmdline - コマンド文字列	
 * return
 *	最後に実行したコマンドのエラーコード
 */
int mySystem( const char *cmdline )
{
    if( cmdline[0] == '\0' )
	return 0;
#ifdef NYADOS
    /* NYADOS の場合は、標準エラー出力・擬似パイプの自前処理のため、
     * もう少し深い階層部分(do_one_command関数)で、system 関数を呼ぶ。
     *
     * NYACUS でそれをしないのは、95,98,Me では本物のパイプの動作が
     * いまいち不安定であるため。
     */
    NnString outempfn;
#else
    NnString *mode=(NnString*)properties.get("standalone");
    if( mode == NULL || mode->length() <= 0 ){
	errno = 0;
	int rc=system( cmdline );
	if( errno != 0 ){
	    perror( SHELL_NAME );
	    return -1;
	}
	return rc;
    }
    int pipefd0 = -1;
    int save0 = -1;
#endif
    NnVector pipeSet;
    int result=0;

    devidePipes( cmdline , pipeSet );
    for( int i=0 ; i < pipeSet.size() ; ++i ){
	errno = 0;
#ifdef NYADOS
	/* NYADOS ではテンポラリファイルをベースとした、
	 * 擬似パイプを構築する。
	 */
	NnString intempfn;
	{
	    Redirect redirect0(0);
	    Redirect redirect1(1);

	    if( ! outempfn.empty() ){
		intempfn = outempfn;
		redirect0.switchTo( intempfn , "r" );
	    }
	    if( i < pipeSet.size()-1 ){
		outempfn = NnDir::tempfn(); 
		redirect1.switchTo( outempfn , "w" );
	    }
	    result = do_one_command( ((NnString*)pipeSet.at(i))->chars() , P_WAIT );
	}
	if( ! intempfn.empty() )
	    ::remove( intempfn.chars() );
#else
	/* NYACUS,NYAOS2 では、本物のパイプをコマンドを連結する */
        if( pipefd0 != -1 ){
	    /* パイプラインが既に作られている場合、
	     * 入力側を標準入力へ張る必要がある
	     */
            if( save0 == -1  )
		save0 = dup( 0 );
            dup2( pipefd0 , 0 );
	    ::close( pipefd0 );
            pipefd0 = -1;
        }
        if( i < pipeSet.size() - 1 ){
	    /* パイプラインの末尾でないコマンドの場合、
	     * 標準出力の先を、パイプの一方にする必要がある
	     */
            int handles[2];

            int save1=dup(1);
#ifdef NYAOS2
            _pipe( handles );
#else
            _pipe( handles , 0 , O_BINARY | O_NOINHERIT );
#endif
            dup2( handles[1] , 1 );
	    ::close( handles[1] );
            pipefd0 = handles[0];

            do_one_command( ((NnString*)pipeSet.at(i))->chars(),P_NOWAIT );

            dup2( save1 , 1 );
            ::close( save1 );
        }else{
            result = do_one_command( ((NnString*)pipeSet.at(i))->chars(),P_WAIT );
        }
#endif
	if( result < 0 ){
#ifdef NYADOS
	    if( ! outempfn.empty() )
		::remove( outempfn.chars() );
#endif
	    if( ((NnString*)pipeSet.at(i))->length() > 110 ){
		conErr << "Too long command line,"
			    " or bad command or file name.\n";
	    }else{
		conErr << "Bad command or file name.\n";
	    }
            goto exit;
	}
    }
  exit:
#ifndef NYADOS
    if( pipefd0 != -1 ){
	::close( pipefd0 );
    }
    if( save0 != -1 ){
        dup2( save0 , 0 );
        ::close( save0 );
    }
#endif
    return result;
}

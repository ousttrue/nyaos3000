#include <assert.h>
#include <io.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#if defined(HAVE_NOT_OLD_NEW_H) || defined(_MSC_VER)
#  include <new>
#else
#  include <new.h>
#endif

#include "config.h"
#include "nnstring.h"
#include "getline.h"
#include "ishell.h"
#include "writer.h"

#define VER "20091129"

#ifdef _MSC_VER
int  nya_new_handler(size_t size)
#else
void nya_new_handler()
#endif
{
    fputs("memory allocation error.\n",stderr);
    abort();
#ifdef _MSC_VER
    return 0;
#endif
}

/* _nya ファイルのパス */
static NnString rcfname;

/* rcファイルを探す.
 *	rcfname - 見つかった場合、rcファイル名が入る
 *	argv0   - exeファイルの名前.
 * return
 *	0 - 見つかった , 1 - 見つからなかった
 */
static int seekrc( NnString &rcfname , const char *argv0 )
{
    const static char *rcfnames[] = RUN_COMMANDS ;
    NnString rcfn;
    for( const char **p =rcfnames ; *p != NULL ; ++p ){
	/* カレントディレクトリの _nya を試みる */
        if( access( *p ,0 ) == 0 ){
            rcfname = *p ;
            return 0;
        }
	/* HOME ディレクトリの _nya を試みる */
        const char *home=getEnv("HOME");
        if( home == NULL )
            home = getEnv("USERPROFILE");

        if( home != NULL ){
            rcfn = home;
            rcfn.trim();
            rcfn << '\\' << *p;
            if( access( rcfn.chars() , 0 )==0 ){
                rcfname = rcfn;
                return 0;
            }
        }
        rcfn  = "\\";
        rcfn += *p;
        if( access(rcfn.chars(),0) == 0 ){
            rcfname = rcfn;
            return 0;
        }
	/* EXE ファイルと同じディレクトリの _nya を試みる */
	int lastroot=NnDir::lastRoot( argv0 );
	if( lastroot != -1 ){
	    rcfn.assign( argv0 , lastroot );
	    rcfn << '\\' << *p;
	    if( access( rcfn.chars(),0) == 0 ){
		rcfname = rcfn;
		return 0;
	    }
	}
    }
    return 1;
}

/* -d デバッグオプション
 *	argc , argv : main の引数
 *	i : 現在読み取り中の argv の添字
 * return
 *	常に 0 (継続)
 */
static int opt_d( int , char **argv , int &i )
{
    if( argv[i][2] == '\0' ){
	properties.put("debug",new NnString() );
    }else{
	size_t len=strcspn(argv[i]+2,"=");
	NnString var(argv[i]+2,len);
	if( argv[i][2+len] == '\0' ){
	    properties.put( var , new NnString() );
	}else{
	    properties.put( var , new NnString(argv[i]+2+len+1));
	}
    }
    return 0;
}
/* -r _nya 指定オプション処理
 *	argc , argv : main の引数
 *	i : 現在読み取り中の argv の添字
 * return
 *	0 : 継続 
 *	not 0 : NYA*S を終了させる(mainの戻り値+1を返す)
 */
static int opt_r( int argc , char **argv, int &i )
{
    if( i+1 >= argc ){
	conErr << "-r : needs a file name.\n";
	return 2;
    }
    if( access( argv[i+1] , 0 ) != 0 ){
	conErr << argv[++i] << ": no such file.\n";
	return 2;
    }
    rcfname = argv[++i];
    return 0;
}
/* -e 1行コマンド用オプション処理
 *	argc , argv : main の引数
 *	i : 現在読み取り中の argv の添字
 * return
 *	0 : 継続 
 *	not 0 : NYA*S を終了させる(mainの戻り値+1を返す)
 */
static int opt_e( int argc , char **argv , int &i )
{
    if( i+1 >= argc ){
	conErr << "-e needs commandline.\n";
	return 2;
    }
    OneLineShell sh( argv[++i] );
    sh.mainloop();
    return 1;
}

/* -f スクリプト実行用オプション処理
 *	argc , argv : main の引数
 *	i : 現在読み取り中の argv の添字
 * return
 *	0 : 継続 
 *	not 0 : NYA*S を終了させる(mainの戻り値+1を返す)
 */
static int opt_f( int argc , char **argv , int &i )
{
    if( i+1 >= argc ){
	conErr << argv[0] << " -f : no script filename.\n";
	return 2;
    }
    if( access(argv[i+1],0 ) != 0 ){
	conErr << argv[i+1] << ": no such file or command.\n";
	return 2;
    }
    NnString path(argv[i+1]);
    ScriptShell scrShell( path.chars() );

    for( int j=i+1 ; j < argc ; j++ )
	scrShell.addArgv( *new NnString(argv[j]) );
    int rc=1;
    if( argv[i][2] == 'x' ){
	NnString line;
	while(   (rc=scrShell.readline(line)) >= 0
	      && (strstr(line.chars(),"#!")  == NULL
	      || strstr(line.chars(),"nya") == NULL ) ){
	    // no operation.
	}
    }
    if( rc >= 0 )
	scrShell.mainloop();
    return 1;
}
/* -a デフォルトエイリアスを有効にする
 *	argc , argv : main の引数
 *	i : 現在読み取り中の argv の添字
 * return
 *	0 : 継続 
 *	not 0 : NYA*S を終了させる(mainの戻り値+1を返す)
 */
static int opt_a( int , char ** , int & )
{
    extern NnHash aliases;

    aliases.put("ls"    , new NnString("list") );
    aliases.put("mv"    , new NnString("move /-Y $@") );
    aliases.put("cp"    , new NnString("copy /-Y /B /V $@") );
    aliases.put("rmdir" , new NnString("rmdir $@") );
    aliases.put("rm"    , new NnString("del $@") );

    return 0;
}

int main( int argc, char **argv )
{
    // set_new_handler(nya_new_handler);
    properties.put("nyatype",new NnString(SHELL_NAME) );

    NnVector nnargv;
    for(int i=1;i<argc;i++){
	if( argv[i][0] == '-' ){
	    int rv;
	    switch( argv[i][1] ){
            case 'D': rv=opt_d(argc,argv,i); break;
	    case 'r': rv=opt_r(argc,argv,i); break;
            case 'f': rv=opt_f(argc,argv,i); break;
	    case 'e': rv=opt_e(argc,argv,i); break;
	    case 'a': rv=opt_a(argc,argv,i); break;
	    default:
		rv = 2;
		conErr << argv[0] << " : -" 
		    << (char)argv[1][1] << ": No such option\n";
		break;
	    }
	    if( rv != 0 )
		return rv-1;
	}else{
            nnargv.append( new NnString(argv[i]) );
	}
    }
    conOut << 
#ifdef ESCAPE_SEQUENCE_OK
"\x1B[2J" << 
#endif
#ifdef NYADOS
	"Nihongo Yet Another DOS Shell "
#elif defined(NYACUS)
	"Nihongo Yet Another OSes Shell-3000 "
#else
	"Nihongo Yet Another OS/2 Shell "
#endif
	"v."VER" (C) 2001-09 by NYAOS.ORG\n";

    NnDir::set_default_special_folder();

    /* _nyados を実行する */
    if( !rcfname.empty() || seekrc(rcfname,argv[0])==0 ){
	rcfname.slash2yen();
	ScriptShell scrShell( rcfname.chars() );

        scrShell.addArgv( rcfname );
        for( int i=0 ; i < nnargv.size() ; i++ ){
            if( nnargv.const_at(i) != NULL )
                scrShell.addArgv( *(const NnString *)nnargv.const_at(i) );
        }
	scrShell.mainloop();
    }

    signal( SIGINT , SIG_IGN );

    /* DOS窓からの入力に従って実行する */
    InteractiveShell intShell;

    /* ヒストリをロードする */
    NnString *histfn=(NnString*)properties.get("savehist");
    if( histfn != NULL ){
	FileReader fr( histfn->chars() );
	if( fr.eof() ){
	    perror( histfn->chars() );
	}else{
	    intShell.getHistoryObject()->read( fr );
	}
    }

    intShell.mainloop();

    /* ヒストリを保存する */
    if( (histfn=(NnString*)properties.get("savehist")) != NULL ){
        int histfrom=0;
        NnString *histsizestr=(NnString*)properties.get("histfilesize");
	History *hisObj=intShell.getHistoryObject();
        if( histsizestr != NULL ){
	    int histsize=atoi(histsizestr->chars());
	    if( histsize !=0 && (histfrom=hisObj->size() - histsize)<0 )
		histfrom = 0;
        }
        FileWriter fw( histfn->chars() , "w" );
        NnString his1;
        for(int i=histfrom ; i<hisObj->size() ; i++ ){
	    if( hisObj->get(i,his1) == 0 )
                (Writer&)fw << his1 << '\n';
        }
    }
    conOut << "\nGood bye!\n";
    return 0;
}


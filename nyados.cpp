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
#include "nua.h"
#include "ntcons.h"
#include "source.h"

#ifndef VER
#define VER "2.93_0"
#endif

#ifdef _MSC_VER
#  include <windows.h>
#endif

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

/* -E 1行Luaコマンド用オプション処理
 * -F ファイル読み込みオプション処理
 */
static int opt_EF( int argc , char **argv , int &i )
{
#ifdef LUA_ENABLE
    if( i+1 >= argc ){
	conErr << "option needs commandline.\n";
	return 2;
    }
    lua_State *lua=nua_init();
    int n=0;

    if( argv[i][1] == 'E' ){
        if( luaL_loadstring( lua , argv[++i] ) )
            goto errpt;
    }else{
        if( luaL_loadfile( lua , argv[++i] ) )
            goto errpt;
    }

    lua_newtable(lua);
    while( ++i < argc ){
        lua_pushinteger(lua,++n);
        lua_pushstring(lua,argv[i]);
        lua_settable(lua,-3);
    }
    lua_setglobal(lua,"arg");

    if( lua_pcall( lua , 0 , 0 , 0 ) )
        goto errpt;

    lua_settop(lua,0);
#else
    shell.err() << "require: built-in lua disabled.\n";
#endif /* defined(LUA_ENABLE) */
    return 1;
#ifdef LUA_ENABLE
  errpt:
    const char *msg = lua_tostring( lua , -1 );
    fputs(msg,stderr);
    putc('\n',stderr);
    return 2;
#endif
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

#ifdef NYACUS
/* -t コンソールの直接のコントロールを抑制し、
 *    ANSIエスケープシーケンスをそのまま出力する
 *	i : 現在読み取り中の argv の添字
 * return
 *	0 : 継続 
 *	not 0 : NYA*S を終了させる(mainの戻り値+1を返す)
 */
static int opt_t( int , char ** , int & )
{
    delete conOut_; conOut_ = new RawWriter(1);
    delete conErr_; conErr_ = new RawWriter(2);

    return 0;
}
#endif

static void goodbye()
{
    lua_State *lua=nua_init();

    lua_getglobal(lua,"nyaos");
    if( lua_istable(lua,-1) ){
        lua_getfield(lua,-1,"goodbye");
        if( lua_isfunction(lua,-1) ){
            if( lua_pcall(lua,0,0,0) != 0 ){
                const char *msg = lua_tostring(lua,-1);
                conErr << "logoff code nyaos.goodbye() raise error.\n"
                       << msg << "\n[Hit any key]\n";
                lua_pop(lua,1);
                (void)Console::getkey();
            }
        }else{
            lua_pop(lua,1); /* drop nil when goodbye not exists */
        }
    }
    lua_pop(lua,1); /* drop 'nyaos' */
    assert( lua_gettop(lua) == 0 );
}

int main( int argc, char **argv )
{
    // set_new_handler(nya_new_handler);
    properties.put("nyatype",new NnString(SHELL_NAME) );
    setvbuf( stdout , NULL , _IONBF , 0 );
    setvbuf( stderr , NULL , _IONBF , 0 );

    NnVector nnargv;

    lua_State *lua = nua_init();
    lua_getglobal(lua,"nyaos"); /* [1] */
    lua_newtable(lua); /* [2] */
    for(int i=1;i<argc;i++){
        lua_pushinteger(lua,i); /* [3] */
        lua_pushstring(lua,argv[i]); /* [4] */
        lua_settable(lua,-3);
	if( argv[i][0] == '-' ){
	    int rv;
	    switch( argv[i][1] ){
            case 'D': rv=opt_d(argc,argv,i); break;
	    case 'r': rv=opt_r(argc,argv,i); break;
            case 'f': rv=opt_f(argc,argv,i); break;
	    case 'e': rv=opt_e(argc,argv,i); break;
	    case 'a': rv=opt_a(argc,argv,i); break;
            case 'F':
            case 'E': rv=opt_EF(argc,argv,i); break;
#ifdef NYACUS
            case 't': rv=opt_t(argc,argv,i); break;
#endif
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
    /* nyaos.argv() == pairs( nyaos.argv ) と等価にする */
    lua_newtable(lua);
    lua_getglobal(lua,"pairs");
    lua_setfield(lua,-2,"__call");
    lua_setmetatable(lua,-2);

    /* argv を nyaos のフィールドに登録する */
    lua_setfield(lua,-2,"argv");
    lua_settop(lua,0);
    conOut << 
#ifdef ESCAPE_SEQUENCE_OK
"\x1B[2J" << 
#endif
	"Nihongo Yet Another OSes Shell 3000 v."VER" (C) 2001-10 by NYAOS.ORG\n";

    NnDir::set_default_special_folder();

    /* _nya を実行する */
    if( rcfname.empty() ){
#ifdef _MSC_VER
        char execname[ FILENAME_MAX ];

        if( GetModuleFileName( NULL , execname , sizeof(execname)-1 ) > 0 ){
            seek_and_call_rc(execname,nnargv);
        }else{
#endif
            seek_and_call_rc(argv[0],nnargv);
#ifdef _MSC_VER
        }
#endif
    }else{
        callrc(rcfname,nnargv);
    }

    signal( SIGINT , SIG_IGN );
#ifdef OS2EMX
    signal( SIGPIPE , SIG_IGN );
#endif

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
    goodbye();
    return 0;
}


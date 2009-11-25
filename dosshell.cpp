#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "ntcons.h"
#include "nnhash.h"
#include "getline.h"
#include "nndir.h"
#include "writer.h"

/* このハッシュテーブルに、拡張子が登録されていると、
 * その拡張子のファイル名は、実行可能と見なされる。
 * (基本的に登録内容は、文字列 => 文字列 が前提)
 * なお、拡張子は小文字に変換して格納すること。
 */
NnHash DosShell::executableSuffix;

/* そのファイル名がディレクトリかどうかを末尾にスラッシュなどが
 * ついているかどうかにより判定する。
 *      path - ファイル名
 * return
 *      非0 - ディレクトリ , 0 - 一般ファイル
 */
static int isDirectory( const char *path )
{
    int lastroot=NnDir::lastRoot(path);
    return lastroot != -1 && path[lastroot+1] == '\0';
}

/* そのファイル名が実行可能かどうかを拡張子より判定する。
 *      path - ファイル名
 * return
 *      非0 - 実行可能 , 0 - 実行不能
 */
static int isExecutable( const char *path )
{
    const char *suffix=strrchr(path,'.');
    /* 拡張子がなければ、実行不能 */
    if( suffix == NULL || suffix[0]=='\0' || suffix[1]=='\0' )
        return 0;
    
    /* 小文字化 */
    NnString sfxlwr(suffix+1);
    sfxlwr.downcase();

    return sfxlwr.compare("com") == 0
        || sfxlwr.compare("exe") == 0
        || sfxlwr.compare("bat") == 0
	|| sfxlwr.compare("cmd") == 0
        || DosShell::executableSuffix.get(sfxlwr) != NULL ;
}

/* コマンド名補完のための候補リストを作成する。
 *      region - 被補完文字列の範囲
 *      array - 補完候補を入れる場所
 * return
 *      候補の数
 */
int DosShell::makeTopCompletionList( const NnString &region , NnVector &array )
{
    NnString pathcore;

    /* 先頭の引用符を除く */
    if( region.at(0) == '"' ){
        pathcore = region.chars() + 1;
    }else{
        pathcore = region;
    }

    makeCompletionList( region , array );
    for(int i=0 ; i<array.size() ; ){
        NnString *fname=(NnString *)((NnPair*)array.at(i))->first();
        if( isExecutable( fname->chars())  ||  isDirectory( fname->chars() )){
            ++i;
        }else{
            array.deleteAt( i );
        }
    }

    if( region.findOf("/\\") != -1 )
        return array.size();
    
    const char *path=getEnv("PATH",NULL);
    if( path == NULL )
        return array.size();

    /* 環境変数 PATH を操作する */
    NnString rest(path);
    while( ! rest.empty() ){
	NnString path1;

	rest.splitTo(path1,rest,";");
	if( path1.empty() )
	    continue;
	path1.dequote();
        path1 += "\\*";
        for( NnDir dir(path1) ; dir.more() ; dir.next() ){
            if(    ! dir.isDir()
                && dir->istartsWith(pathcore) 
                && isExecutable(dir.name()) )
            {
                if( array.append(new NnPair(new NnStringIC(dir.name()))) )
                    break;
            }
        }
        /* エイリアスを見にゆく : (注意)グローバル変数を参照している */
        extern NnHash aliases;
        for( NnHash::Each p(aliases) ; p.more() ; p.next() ){
            if( p->key().istartsWith(pathcore) ){
                if( array.append( new NnPair(new NnStringIC(p->key()) )) )
                    break;
            }
        }
    }
    array.sort();
    array.uniq();
    return array.size();
}

void DosShell::putchr(int ch)
{
    conOut << (char)ch;
}

void DosShell::putbs(int n)
{
    while( n-- > 0 )
	conOut << (char)'\b';
}

int DosShell::getkey()
{
    fflush(stdout);
#ifdef NYACUS
    conOut << "\x1B[>5l";
#endif

    int ch=Console::getkey();
    if( isKanji(ch) ){
#if 1
	if( ch ==0xE0 ) /* xscript */
	    ch = 0x01;
#endif
        ch = (ch<<8) | Console::getkey() ;
    }else if( ch == 0 ){
        ch = 0x100 | Console::getkey() ;
    }
    return ch;
}

/* 編集開始フック */
void DosShell::start()
{
    prompt();
}

/* 編集終了フック */
void DosShell::end()
{
    putchar('\n');
}

#ifdef PROMPT_SHIFT
/* プロンプトのうち、nカラム目からを取り出す
 *      prompt - 元文字列
 *      offset - 取り出す位置
 *      result - 取り出した結果
 * return
 *      抽出プロンプトの桁数
 */
static int prompt_substr( const NnString &prompt , int offset , NnString &result )
{
    int i=0;
    int column=0;
    int nprints=0;
    int knj=0;
    for(;;){
        if( prompt.at(i) == '\x1B' ){
            for(;;){
                if( prompt.at(i) == '\0' )
                    goto exit;
                result << (char)prompt.at(i);
                if( isAlpha(prompt.at(i++)) )
                    break;
            }
        }else if( prompt.at(i) == '\0' ){
            break;
        }else{
            if( column == offset ){
                if( knj )
                    result << ' ';
                else
                    result << (char)prompt.at(i);
                ++nprints;
            }else if( column > offset ){
                result << (char)prompt.at(i);
                ++nprints;
            }
            if( knj == 0 && isKanji(prompt.at(i)) )
                knj = 1;
            else
                knj = 0;
            ++i;
            column++;
        }
    }
  exit:
    return nprints;
}


#else
/* エスケープシーケンスを含まない文字数を得る. 
 *      p - 文字列先頭ポインタ
 * return
 *      文字列のうち、エスケープシーケンスを含まない分の長さ
 *      (純粋な桁数)
 */
static int strlenNotEscape( const char *p )
{
    int w=0,escape=0;
    while( *p != '\0' ){
        if( *p == '\x1B' )
            escape = 1;
        if( ! escape )
            ++w;
        if( isAlpha(*p) )
            escape = 0;
	if( *p == '\n' )
	    w = 0;
        ++p;
    }
    return w;
}
#endif

/* プロンプトを表示する.
 * return プロンプトの桁数(バイト数ではない=>エスケープシーケンスを含まない)
 */
int DosShell::prompt()
{
    const char *sp=prompt_.chars();
    NnString prompt;

    time_t now;
    time( &now );
    struct tm *thetime = localtime( &now );
    
    if( sp != NULL && sp[0] != '\0' ){
        while( *sp != '\0' ){
            if( *sp == '$' ){
                ++sp;
                switch( toupper(*sp) ){
                    case '_': prompt << '\n';   break;
                    case '$': prompt << '$'; break;
		    case 'A': prompt << '&'; break;
                    case 'B': prompt << '|'; break;
		    case 'C': prompt << '('; break;
                    case 'D':
			prompt.addValueOf(thetime->tm_year + 1900);
			prompt << '-';
			if( thetime->tm_mon + 1 < 10 )
			    prompt << '0';
			prompt.addValueOf(thetime->tm_mon + 1);
			prompt << '-';
			if( thetime->tm_mday < 10 )
			    prompt << '0';
			prompt.addValueOf( thetime->tm_mday );
                        switch( thetime->tm_wday ){
                            case 0: prompt << " (日)"; break;
                            case 1: prompt << " (月)"; break;
                            case 2: prompt << " (火)"; break;
                            case 3: prompt << " (水)"; break;
                            case 4: prompt << " (木)"; break;
                            case 5: prompt << " (金)"; break;
                            case 6: prompt << " (土)"; break;
                        }
                        break;
                    case 'E': prompt << '\x1B'; break;
		    case 'F': prompt << ')'; break;
                    case 'G': prompt << '>';    break;
                    case 'H': prompt << '\b';   break;
                    case 'I': break;
                    case 'L': prompt << '<';    break;
		    case 'N': prompt << (char)NnDir::getcwdrive(); break;
                    case 'P':{
                        NnString pwd;
                        NnDir::getcwd(pwd) ;
                        prompt << pwd; 
                        break;
                    }
                    case 'Q': prompt << '='; break;
		    case 'S': prompt << ' ';    break;
                    case 'T':
			if( thetime->tm_hour < 10 )
			    prompt << '0';
			prompt.addValueOf(thetime->tm_hour);
			prompt << ':';
			if( thetime->tm_min < 10 )
			    prompt << '0';
			prompt.addValueOf(thetime->tm_min);
			prompt << ':';

			if( thetime->tm_sec < 10 )
			    prompt << '0';
			prompt.addValueOf(thetime->tm_sec) ;
                        break;
                    case 'V':
			prompt << SHELL_NAME ; break;
                    case 'W':{
                        NnString pwd;
                        NnDir::getcwd(pwd);
			int rootpos=pwd.findLastOf("/\\");
			if( rootpos == -1 || rootpos == 2 ){
                            prompt << pwd;
			}else{
                            prompt << (char)pwd.at(0) << (char)pwd.at(1);
                            prompt << pwd.chars()+rootpos+1;
			}
			break;
                    }
                    default:  prompt << '$' << *sp; break;
                }
                ++sp;
            }else{
                prompt += *sp++;
            }
        }
    }else{
        NnString pwd;
        NnDir::getcwd(pwd);
        prompt << pwd << '>';
    }

#ifdef PROMPT_SHIFT
    NnString prompt2;
    prompt_size = prompt_substr( prompt , prompt_offset , prompt2 );
    conOut << prompt2;
#else
    int prompt_size = strlenNotEscape( prompt.chars() );
    conOut << prompt;
#endif

    /* 必ずキープしなくてはいけない編集領域のサイズを取得する */
    NnString *temp;
    int minEditWidth = 10;
    if(    (temp=(NnString*)properties.get("mineditwidth")) != NULL
        && (minEditWidth=atoi(temp->chars())) < 1 )
    {
        minEditWidth = 10;
    }

    int whole_width=DEFAULT_WIDTH;
    if(    (temp=(NnString*)properties.get("width")) != NULL
        && (whole_width=atoi(temp->chars())) < 1 )
    {
        whole_width = DEFAULT_WIDTH;
    }
#ifdef NYACUS
    else {
        whole_width = Console::getWidth();
    }
#endif

    int width=whole_width - prompt_size % whole_width ;
    if( width <= minEditWidth ){
	conOut << '\n';
        width = whole_width;
    }
    setWidth( width );
    return width;
}

void DosShell::clear()
{
    conOut << "\x1B[2J";
}

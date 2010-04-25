#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <io.h>
#include "shell.h"
#include "nnstring.h"
#include "nnhash.h"
#include "getline.h"
#include "reader.h"

/* 逆クォートで読みこめる文字列量を戻す */
static int getQuoteMax()
{
    NnString *max=(NnString*)properties.get("backquote");
    return max == NULL ? 0 : 1024 ;
}

/* コマンドの標準出力の内容を文字列に取り込む. 
 *   cmdline - 実行するコマンド
 *   dst - 標準出力の内容
 *   max - 引用の上限. 0 の時は上限無し
 *   shrink - 二つ以上の空白文字を一つにする時は true とする
 * return
 *   0  : 成功
 *   -1 : オーバーフロー(max までは dst に取り込んでいる)
 */
int eval_cmdline( const char *cmdline, NnString &dst, int max , bool shrink)
{
    /* テンポラリファイルを使って実現 */
    NnString tempfn=NnDir::tempfn();
    FILE *fp=fopen( tempfn.chars() , "w" );
    int tmpfd = dup(1);
    dup2( fileno(fp) , 1 );

    OneLineShell shell( cmdline );
    shell.mainloop();

    dup2( tmpfd , 1 );
    ::close( tmpfd );
    fclose(fp);

    FileReader pp( tempfn.chars() );
    
    int lastchar=' ';
    int ch;
    while( !pp.eof() && (ch=pp.getchr()) != EOF ){
        if( shrink && isSpace(ch) ){
            if( ! isSpace(lastchar) )
                dst += ' ';
        }else{
            dst += (char)ch;
        }
        lastchar = ch;
        if( max && dst.length() >= max )
            return -1;
    }
    dst.trim();
    ::remove( tempfn.chars() );
    return 0;
}

/* 逆クォート処理
 *      sp  元文字列(`の次を指していること)
 *      dst 先文字列
 *      max 上限
 * return
 *      0  成功
 *      -1 オーバーフロー
 */
static int doQuote( const char *&sp , NnString &dst , int max )
{
    NnString q;
    while( *sp != '\0' ){
        if( *sp =='`' ){
            /* 連続する `` は、一つの ` へ変換する。 */
            if( *(sp+1) != '`' )
                break;
	    ++sp;
        }
	if( isKanji(*sp) )
	    q += *sp++;
        q += *sp++;
    }
    if( q.length() <= 0 ){
        dst += '`';
        return 0;
    }
    return eval_cmdline( q.chars() , dst , max , true );
}


/* リダイレクト先を読み取り、結果を FileWriter オブジェクトで返す.
 * オープンできない場合はエラーメッセージも勝手に出力.
 *	sp 最初の'>'の直後
 *         実行後は、
 * return
 *      出力先
 */
static FileWriter *readWriteRedirect( const char *&sp )
{

    // append ?
    const char *mode = "w";
    if( *sp == '>' ){
	++sp;
	mode = "a";
    }
    NnString fn;
    NyadosShell::readNextWord(sp,fn);
    if( fn.empty() ){
        conErr << "syntax error near unexpected token `newline'\n";
	return NULL;
    }

    FileWriter *fw=new FileWriter(fn.chars(),mode);

    if( fw != NULL && fw->ok() ){
	return fw;
    }else{
	perror( fn.chars() );
	delete fw;
	return NULL;
    }
}

/* sp が特殊フォルダーを示す文字列から始まっていたら真を返す
 *	sp … 先頭位置
 *	quote … 二重引用符内なら真にする
 *	spc … 直前の文字が空白なら真にしておく
 * return
 *	doFolder で変換すべき文字列であれば、その長さ
 */
int isFolder( const char *sp , int quote , int spc )
{
    if( ! spc  ||  quote )
	return 0;

    if( *sp=='~' && 
        (getEnv("HOME")!=NULL || getEnv("USERPROFILE") != NULL ) && 
        properties.get("tilde")!=NULL)
    {
	int n=1;
	while( isalnum(sp[n] & 255) || sp[n]=='_' )
	    ++n;
	return n;
    }

    if( sp[0]=='.' && sp[1]=='.' && sp[2]=='.' && properties.get("dots")!=NULL ){
	int n=3;
	while( sp[n] == '.' )
	    ++n;
	return n;
    }
    return 0;
}
/* sp で始まる特殊フォルダー名を本来のフォルダー名へ変換する
 *	sp … 先頭位置
 *	len … 長さ
 *	dst … 本来のフォルダー名を入れるところ
 *	quote … 二重引用符内なら真にする
 */	
static void doFolder( const char *&sp , int len , NnString &dst , int & )
{
    NnString name(sp,len),value;
    NnDir::filter( name.chars() , value );
    sp += len;
    if( value.findOf(" \t\v\r\n") >= 0 ){
	dst << '"' << value << '"';
    }else{
	dst << value;
    }
}

/* 環境変数などを展開する(内蔵コマンド向け)
 *	src - 元文字列
 *	dst - 結果が入る
 * return
 *      0 - 成功 , -1 - 失敗
 */
int NyadosShell::explode4internal( const NnString &src , NnString &dst )
{
    /* パイプまわりの処理を先に実行する */
    NnString firstcmd; /* 最初のパイプ文字までのコマンド */
    NnString restcmd;  /* それ移行のコマンド */
    src.splitTo( firstcmd , restcmd , "|" );
    if( ! restcmd.empty() ){
	if( restcmd.at(0) == '&' ){ /* 標準出力+エラー出力 */
	    NnString pipecmds( restcmd.chars()+1 );

            PipeWriter *pw=new PipeWriter(pipecmds);
            if( pw == NULL || ! pw->ok() ){
                perror( pipecmds.chars() );
                delete pw;
                return -1;
	    }
            conOut_ = pw ;
            conErr_ = new WriterClone(pw);
	}else{ /* 標準出力のみ */
            NnString pipecmds( restcmd.chars()+1 );

            PipeWriter *pw=new PipeWriter(pipecmds);
            if( pw == NULL || ! pw->ok() ){
                perror( pipecmds.chars() );
                delete pw;
                return -1;
	    }
            conOut_ = pw;
	}
    }
    int prevchar=' ';
    int quote=0;
    int len;
    dst.erase();
    int backquotemax=getQuoteMax();

    const char *sp=firstcmd.chars();
    while( *sp != '\0' ){

	if( *sp == '"' )
	    quote = !quote;
	
	int prevchar1=(*sp & 255);
	if( (len=isFolder(sp,quote,isSpace(prevchar))) != 0 ){
	    doFolder( sp , len , dst , quote );
        }else if( *sp == '>' && !quote ){
	    ++sp;
	    FileWriter *fw=readWriteRedirect( sp );
	    if( fw != NULL ){
                conOut_ = fw;
	    }else{
		conErr << "can't redirect stdout.\n";
		return -1;
	    }
	}else if( *sp == '1' && *(sp+1) == '>' && !quote ){
	    if( ! restcmd.empty() ){
		/* すでにリダイレクトしていたら、エラー */
		conErr << "ambigous redirect.\n";
		return -1;
	    }
	    sp += 2;
	    if( *sp == '&'  &&  *(sp+1) == '2' ){
		sp += 2;
                conOut_ = new WriterClone(conErr_);
	    }else if( *sp == '&' && *(sp+1) == '-' ){
		sp += 2;
                conOut_ = new NullWriter();
	    }else{
		FileWriter *fw=readWriteRedirect( sp );
		if( fw != NULL ){
                    conOut_ = fw;
		}else{
		    conErr << "can't redirect stdout.\n";
		    return -1;
		}
	    }
	}else if( *sp == '2' && *(sp+1) == '>' && !quote ){
	    sp += 2;
	    if( *sp == '&' && *(sp+1) == '1' ){
		sp += 2;
                conErr_ = new WriterClone(conOut_);
	    }else if( *sp == '&' && *(sp+1) == '-' ){
		sp += 2;
                conErr_ = new NullWriter();
	    }else{
		FileWriter *fw=readWriteRedirect( sp );
		if( fw != NULL ){
                    conErr_ = fw;
		}else{
		    conErr << "can't redirect stderr.\n";
		    return -1;
		}
	    }
        }else if( *sp == '`' && backquotemax > 0 ){
	    ++sp;
            if( doQuote( sp , dst , backquotemax ) == -1 ){
                conErr << "over flow commandline.\n";
                return -1;
            }
	    ++sp;
	}else{
	    if( isKanji(*sp) )
		dst += *sp++;
	    dst += *sp++;
	}
	prevchar = prevchar1;
    }
    return 0;
}

/* ヒアドキュメントを行う
 *    sp - 「<<END」の最初の < の位置を差す。
 *    dst - 結果(「< 一時ファイル名」等)が入る
 *    prefix - 「<」or「 」
 */
void NyadosShell::doHereDocument( const char *&sp , NnString &dst , char prefix )
{
    int quote_mode = 0 , quote = 0 ;
    sp += 2;

    /* 終端マークを読み取る */
    NnString endWord;
    while( *sp != '\0' && (!isspace(*sp & 255) || quote ) ){
	if( *sp == '"' ){
	    quote ^= 1;
	    quote_mode = 1;
	}else{
	    endWord << *sp ;
	}
	++sp;
    }
    /* ドキュメント部分を一時ファイルに書き出す */
    this->heredocfn=NnDir::tempfn();
    FILE *fp=fopen( heredocfn.chars() , "w" );
    if( fp != NULL ){
	VariableFilter  variable_filter(*this);
	NnString  line;

	NnString *prompt=new NnString(endWord);
	*prompt << '>';
	this->nesting.append( prompt );

	while( this->readline(line) >= 0 && !line.startsWith(endWord) ){
	    /* <<"EOF" ではなく、<<EOF 形式の場合は、
	     * %...% 形式の単語を置換する必要あり
	     */
	    if( ! quote_mode )
		variable_filter( line );

	    fputs( line.chars() , fp );
	    if( ! line.endsWith("\n") )
		putc('\n',fp);
	    line.erase();
	}
	delete this->nesting.pop();
	fclose( fp );
	dst << prefix << heredocfn ;
    }
}


/* 外部コマンド用プリプロセス.
 * ・%1,%2 を変換する
 * ・プログラム名の / を ￥へ変換する。
 *      src - 元文字列
 *      dst - 加工した結果文字列の入れ先
 * return 0 - 成功 , -1 失敗(オーバーフローなど)
 */
int NyadosShell::explode4external( const NnString &src , NnString &dst )
{
    /* プログラム名をフィルターにかける：チルダ変換など */
    NnString progname,args;
    src.splitTo( progname,args );
    NnDir::filter( progname.chars() , dst );
    if( args.empty() )
	return 0;

    dst << ' ';
    int backquotemax=getQuoteMax();
    int quote=0;
    int len;

    // 引数のコピー.
    int spc=1;
    for( const char *sp=args.chars() ; *sp != '\0' ; ++sp ){
        if( *sp == '"' )
            quote ^= 1;

	if( (len=isFolder(sp,quote,spc)) != 0 ){
	    doFolder( sp , len , dst , quote );
	    --sp;
        }else if( *sp == '`'  && backquotemax > 0 ){
	    ++sp;
            if( doQuote( sp , dst , backquotemax ) == -1 ){
                conErr << "Over flow commandline.\n";
                return -1;
            }
	}else if( *sp == '<' && *(sp+1) == '<'  &&  quote == 0 ){
	    /* ヒアドキュメント */
	    doHereDocument( sp , dst , '<' );
	}else if( *sp == '<' && *(sp+1) == '=' && quote == 0 ){
	    /* インラインファイルテキスト */
	    doHereDocument( sp , dst , ' ' );
	}else if( *sp == '|' && *(sp+1) == '&' ){
	    dst << "2>&1 |";
        }else{
	    if( isKanji(*sp) )
		dst += *sp++;
            dst += *sp;
        }
        spc = isSpace( *sp );
    }
    return 0;
}

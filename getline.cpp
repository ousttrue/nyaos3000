#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#undef  CTRL
#define CTRL(x) ((x) & 0x1F)

#include "getline.h"
#include "nnhash.h"
#include "nua.h"

History GetLine::history;

Status (GetLine::*GetLine::which_command(int key))(int key)
{
    return (unsigned)key>=numof(bindmap) 
        ? &GetLine::insert 
        : bindmap[(unsigned)key];
}
Status GetLine::interpret(int key)
{
    return (this->*this->which_command(key))(key);
}

/* バッファ上のある範囲を表示する。
 * 末尾で倍角文字が分断されるときはダミー文字が表示される。
 *      from - 表示開始位置(バッファ上での位置)
 *      to   - 表示終了位置(バッファ上での位置)
 * [from,to-1]が表示される。
 * return
 *      表示した文字のバイト数(桁数)
 */
int GetLine::puts_between(int from,int to)
{
    if( from <  offset )         from = offset;
    if( to   >  offset+width )   to   = offset+width;

    /* 1文字目 */
    if( to-from > 0 )
        putchr( buffer.isKnj2(from) ? '^' : buffer[ from ] );

    /* 2文字目～n-1文字目 */
    for(int i=from+1 ; i < to-1 ; i++)
        putchr( buffer[i] );

    /* n文字目 */
    if( to-from > 1 )
        putchr( buffer.isKnj1(to-1) ? '$' : buffer[to-1] );
    
    return to-from <= 0 ? 0 : to-from ;
}

int GetLine::putspc(int n)
{
    for(int i=0;i<n;i++)
        putchr(' ');
    return n;
}


/* カーソル以降を再描画 */
void GetLine::repaint_after(int rm)
{
    int bs=puts_between(pos,buffer.length());
    while( rm-- > 0  && (pos+bs-offset) < width ){
        putchr( ' ' );
        ++bs;
    }
    putbs( bs );
}


/* カーソル位置に文字列を挿入し、表示を更新する。
 *      s - 文字列
 * return
 *      挿入バイト数
 */
int GetLine::insertHere( const char *s )
{
    int n=buffer.insert(s,pos);
    if( n <= 0 )
        return 0; // memory allocation error.

    if( pos+n < offset+width ){
        // 挿入文字列の末尾が画面幅に収まる場合、そのまま表示する.
        pos += puts_between( pos , pos+n );
        repaint_after();
    }else{
        /* 挿入文字列の末尾が画面幅を超える場合
         * 先頭を計算しなおして、全体を表示しなおす。
         */
        putbs( pos-offset );
        offset = (pos+=n)-width;
        puts_between( head() , pos );
    }
    return n;
}

/* 一行入力メイン関数(1).
 *	result - [in] 入力のデフォルト値 [out] 入力結果
 * return
 *	0以上 - 文字列の長さ
 *	-1    - キャンセルされた
 *	-2    - その他のエラー
 */
Status GetLine::operator() ( NnString &result )
{
    if( buffer.init() )
        return TERMINATE;

    pos = offset = 0;
    width = DEFAULT_WIDTH ;

    history_pointor = history.size();
    NnString foo;
    history << foo;

    start(); /* ← プロンプトが表示される */

    /* デフォルト値がある場合 */
    if( result.length() > 0 ){
        if( buffer.insert( result.chars(),0 ) > 0 ){
            // 挿入が成功した場合のみ:
            if( (pos=buffer.length()) >= width )
                offset = buffer.length()-width;
        
            puts_between( offset , buffer.length() );
        }
    }

    lastKey = 0;
    
    for(;;){
        int key=getkey();
        Status rc=CONTINUE;
#ifdef LUA_ENABLE
        NyaosLua L(NULL); /* stacked [nyaos] */
        if( L.ok() ){
            lua_getfield(L,-1,"keyhook"); /* [nyaos,keyhook] */
            if( lua_type(L,-1) == LUA_TFUNCTION ){
                int start=lua_gettop(L);  /* [nyaos,keyhook] */

                lua_newtable(L);
                lua_pushinteger(L,key);
                lua_setfield(L,-2,"key");
                lua_pushinteger(L,pos);
                lua_setfield(L,-2,"pos");
                lua_pushstring(L,buffer.chars());
                lua_setfield(L,-2,"text"); /* [nyaos,keyhook,T] */

                if( lua_pcall(L,1,LUA_MULTRET,0) == 0 ){
                    /* keyhook までが pop され [nyaos,r1,r2...] */
                    int n=lua_gettop(L);
                    if( n < start ){ /* 引数 0 個 */
                        rc = interpret( key );
                    }else{
                        for(int i=start ; i<=n && rc==CONTINUE ;i++){
                            switch( lua_type(L,i) ){
                            default:
                            case LUA_TNIL:
                                rc = interpret( key );
                                break;
                            case LUA_TBOOLEAN:
                                rc = lua_toboolean(L,i) ? ENTER : CANCEL;
                                break;
                            case LUA_TNUMBER:
                                rc = interpret( lua_tointeger(L,i) );
                                break;
                            case LUA_TSTRING:
                                this->insertHere( lua_tostring(L,i) );
                                break;
                            }
                        }
                    }
                }else{
                    /* messaging error */
                    /* keyhook までが pop され [nyaos,message] */
                    putchr('\n');
                    const char *p=lua_tostring(L,-1);
                    while( p != NULL && *p != '\0' ){
                        putchr(*p++);
                    }
                    putchr('\n');
                    prompt();
                    puts_between( offset , pos );
                    repaint_after();
                }
            }else{
                rc = interpret(key);
            }
        }else{
#endif
            rc = interpret(key);
#ifdef LUA_ENABLE
        }
#endif
	int len;
        switch( rc ){
        case CONTINUE:
            break;

        case TERMINATE:
        case CANCEL:
            buffer.term();
            end();
            result.erase();
            history.drop();
            return rc;
          
        case ENTER:
            if( buffer.length() > 0 ){
		len = buffer.decode( result );
		NnString *r=history.top();
		if( r != NULL )
		    *r = result;
                history.pack();
            }else{
		len = 0;
                history.drop();
                result = "";
            }
            end();
            buffer.term();
            return rc;
        }
        lastKey = key;
    }
}

void GetLine::replace_all_repaint( const NnString &s )
{
    // プロンプト直後にカーソルを戻す.
    putbs( pos-offset );

    // それまでの表示文字数をカウント.
    int clearSize=printedSize();

    // 現在のヒストリカーソルの内容を編集内容と置き換える.
    buffer.replace( 0,buffer.length(),s.chars() );

    pos = buffer.length();
    offset = 0;

    // 表示幅よりも文字数が大きければ、表示トップをずらす.
    if( pos+1 >= width )
        offset = pos+1-width;
    
    puts_between( offset , buffer.length() );

    int diff = clearSize - printedSize() ;
    for( int bs=0 ; bs < diff ; bs++ )
        putchr(' ');
    putbs(diff);
}

/* 単語を抽出する
 *      m - 行位置
 *      n - 語数位置
 *      word - 検索単語
 */
void GetLine::get_nline_nword(int m,int n,NnString &word)
{
    NnString line=*history[m];
    NnString left;

    line.splitTo( word , left);
    for(int i=0 ; i< n ; i++ ){
        left.splitTo( word , left );
    }
}

/* 行単位でヒストリを検索する
 *      m - ヒストリ番号
 *      line - 検索キー
 */
int GetLine::seekLineForward(int &m, const char *line)
{
    for(;;){
        if( --m < 0 )
            return -1;
        if( history[m]->startsWith(line) )
            return 0;
    }
}
/* 行単位でヒストリを検索する
 *      m - ヒストリ番号
 *      line - 検索キー
 */
int GetLine::seekLineBackward(int &m, const char *line )
{
    for(;;){
        if( ++m >= history.size() )
            return -1;
        if( history[m]->startsWith(line) )
            return 0;
    }
}

/* 単語を検索する
 *      m - 行位置
 *      n - 語数位置
 *      word - 検索単語
 * return
 *      0 - 見つかった
 *      -1 - 見つからなかった
 */
int GetLine::seekWordForward(int &m,int &n,const NnString &word , NnString &found)
{
    for(;;){
        get_nline_nword(m,++n,found);
        if( found.empty() ){ /* 末尾に移動 */
            if( --m < 0 )
                return -1;
            n=0;
        }else if( found.startsWith(word) ){
            return 0;
        }
    }
}
static int count_words(const NnString &line)
{
    int i=0;
    int quote=0;
    int nwords=0;
    for(;;){
        for(;;){
            if( i >= line.length() )
                return nwords;
            if( ! isSpace(line.at(i)) )
                break;
            ++i;
        }
        ++nwords;
        for(;;){
            if( i >= line.length() )
                return nwords;
            if( !quote && isSpace(line.at(i)) )
                break;
            if( line.at(i)=='"')
                quote = !quote;
            ++i;
        }
    }
}

/* 未来方向へ単語を検索する
 *      m - 行位置
 *      n - 語数位置
 *      word - 検索単語
 * return
 *      0 - 見つかった
 *      -1 - 見つからなかった
 */
int GetLine::seekWordBackward(int &m,int &n,const NnString &word,NnString &found)
{
    for(;;){
        while( --n <= 0 ){
            if( ++m >= history.size() )
                return -1;
            n = count_words(*history[m]);
        }
        get_nline_nword(m,n,found);
        if( found.startsWith(word) )
            return 0;
    }
}

/* printSeal で表示したシールを消す */
void GetLine::eraseSeal( int sealsize )
{
    putbs( sealsize );
    if( offset > pos ){
	offset = pos;
    }
    repaint_after(sealsize);
}

/* カーソル位置からシールを表示する。
 * シールは repaint_after を実行することで消去できる。
 *      seal 表示するシール
 *      sealsize  前回の表示したシールサイズ
 *           (上書きできなかった部分を空白にする)
 * return
 *      シールサイズ
 */
int GetLine::printSeal( const char *seal , int sealsize )
{
    putbs( sealsize );
    int seallen = TwinBuffer::strlen_ctrl(seal);
    if( width+offset < pos + seallen ){
        /* シールを表示する空間がない場合は、
         * 左へ適当にスクロールさせる。
         *    元々 offset < pos < offset+width < pos + seallen
         */
        putbs( pos - offset );
        offset = pos + seallen - width;
        if( offset < 0 )
            offset = 0;
        puts_between( offset , pos );
    }
    int bs=0;
    int i=pos;
    while( *seal != '\0' ){
	if( TwinBuffer::isCtrl(*seal) ){
	    if( bs+1 >= width || i+1 >= offset+width )
		break;
	    putchr( '^' );
	    putchr( '@' + *seal++ );
	    bs += 2;
	    i  += 2;
	}else{
	    if( bs >= width || i >= offset+width)
		break;
	    putchr( *seal++ );
	    ++bs;
	    ++i;
	}
    }
    int bs2=bs;
    while( i < offset+width  &&  bs2 < sealsize && bs2 < width ){
        putchr( ' ' );
        ++bs2;
        ++i;
    }
    if( bs2 > bs )
        putbs( bs2-bs );
    return bs;
}

GetLine::~GetLine(){}

int GetLine::prompt(){ return 0;}

/* カーソル上の単語の範囲取得.
 * out:
 *	at - 単語の先頭
 *	size - 単語の長さ
 * return:
 *	文字列自身
 */
NnString GetLine::current_word(int &at,int &size)
{
    NnString *temp=(NnString *)properties.get("uncompletechar");
    const char *dem="";
    if( temp != NULL ){
	temp->trim();
	dem = temp->chars();
    }

    int inQuote=0;
    int j=0;
    at = 0;
    for(;;){
        for(;;){ /* 空白のスキップ */
            if( j >= pos ){
		/* ここは goto done ではなく、at を変更させるべく、
		 * break にしておかなくてはいけない */
		break;
	    }
            if( ! isSpace(buffer[j] & 255) && strchr(dem,buffer[j] & 255) == NULL )
                break;
            ++j;
        }
        at = j;
        for(;;){
            if( j >= pos )
                goto done;
            if( !inQuote ){
                if( isSpace(buffer[j]) || strchr(dem,buffer[j]) != NULL )
                    break;
            }
            if( buffer[j] == '"' )
                inQuote = !inQuote;
            ++j;
        }
    }
  done:
    size = j - at;
  
    NnString result;
    buffer.decode(at,size,result);
    return result;
}


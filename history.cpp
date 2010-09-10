#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "nnstring.h"
#include "nnvector.h"
#include "history.h"
#include "reader.h"
#include "shell.h"

/* ヒストリの中から、特定文字列を含んだ行を取り出す
 *	key - 検索キー
 *	line - ヒットした行
 * return 0 - 見つかった , -1 - 見つからなかった.
 */
int History::seekLineHas( const NnString &key , NnString &line )
{
    NnString temp;
    for( int j=size()-2 ; j >= 0 ; --j ){
	this->get( j , temp );
	if( strstr(temp.chars(),key.chars()) != NULL ){
	    line = temp;
	    return 0;
	}
    }
    return -1;
}
/* ヒストリの中から、特定文字列から始まった行を取り出す
 *	key - 検索キー
 *	line - ヒットした行
 * return 0 - 見つかった , -1 - 見つからなかった.
 */
int History::seekLineStartsWith( const NnString &key , NnString &line )
{
    NnString temp;
    for( int j=size()-2 ; j >= 0 ; --j ){
	this->get( j , temp );
	if( strncmp(temp.chars(),key.chars(),key.length())==0 ){
	    line = temp;
	    return 0;
	}
    }
    return -1;
}

/* 文字列からヒストリ記号を検出して、置換を行う。
 *      hisObj - ヒストリオブジェクト
 *      src - 元文字列
 *      dst - 置換結果を入れる先 or エラー文字列
 * return
 *	0 - 正常終了 , -1 - エラー
 */
int preprocessHistory( History &hisObj , const NnString &src , NnString &dst )
{
    NnString line;
    int quote=0;
    const char *sp=src.chars();

    while( *sp != '\0' ){
	if( *sp != '!' || quote ){
            if( *sp == '"' )
                quote = !quote;
            dst += *sp++;
        }else{
            NnVector argv;
            NnString *arg1;

	    if( *++sp == '\0' )
                break;
            
            int sign=0;
            switch( *sp ){
            case '!':
                ++sp;
            case '*':case ':':case '$':case '^':
		if( hisObj.size() >= 2 ){
		    hisObj.get(-2,line);
                }else{
                    line = "";
		}
                break;
            case '-':
                sign = -1;
		++sp;
            default:
                if( isDigit(*sp) ){
		    int number = (int)strtol(sp,(char**)&sp,10);
		    hisObj.get( sign ? -number-1 : +number , line );
                }else if( *sp == '?' ){
                    NnString key,temp;
                    while( *sp != '\0' ){
                        if( *sp == '?' ){
			    ++sp;
                            break;
                        }
                        key += *sp;
                    }
		    if( hisObj.seekLineHas(key,line) != 0 ){
			dst.erase();
			dst << "!?" << key << ": event not found.\n";
			return -1;
		    }
                }else{
                    NnString key;
		    NyadosShell::readWord(sp,key);

		    if( hisObj.seekLineStartsWith(key,line) != 0 ){
			dst.erase();
			dst << "!" << key << ": event not found.\n";
			return -1;
		    }
                }
                break;
            }
	    line.splitTo( argv );

            if( *sp == ':' )
                ++sp;

            switch( *sp ){
            case '^':
                if( argv.size() > 1 && (arg1=(NnString*)argv.at(1)) != NULL )
                    dst += *arg1;
		++sp;
                break;
            case '$':
                if( argv.size() > 0 
                    && (arg1=(NnString*)argv.at(argv.size()-1)) != NULL )
                    dst += *arg1;
		++sp;
                break;
            case '*':
                for(int j=1;j<argv.size();j++){
                    arg1 = (NnString*)argv.at( j );
                    if( arg1 != NULL )
                        dst += *arg1;
                    dst += ' ';
                }
		++sp;
                break;
            default:
                if( isDigit(*sp) ){
		    int n=strtol(sp,(char**)&sp,10);
                    if( argv.size() >= n && (arg1=(NnString*)argv.at(n)) != NULL )
                        dst += *arg1;
                }else{
                    dst += line;
                }
                break;
            }
        }
    }
    return 0;
}

NnString *History::operator[](int at)
{
    if( at >= size() )
	return NULL;
    if( at >= 0 )
        return (NnString *)this->at(at);
    /* at が負の場合 */
    at += size();
    if( at < 0 )
	return NULL;
    return (NnString *)this->at( at );
}

void History::pack()
{
    NnString *r1=(*this)[-1];
    NnString *r2=(*this)[-2];
    if( size() >= 2 && r1 != NULL && r2 != NULL && r1->compare( *r2 ) == 0 )
	delete pop();
}

History &History::operator << ( const NnString &str )
{
    this->append( str.clone() );
    return *this;
}

void History::read( Reader &reader )
{
    NnString buffer;
    while( reader.readLine(buffer) >= 0 ){
	this->append( buffer.clone() );
    }
}

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


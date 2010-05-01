#ifndef CONST_H
#define CONST_H

// コマンドの戻り値
typedef enum {
    CONTINUE ,	// 入力継続 … そのまま、編集を続ける。
    ENTER ,	// 入力終結 … 編集を終了し、入力文字列をユーザーへ渡す。
    CANCEL ,	// 入力破棄 … 編集を終了するが、入力文字列は破棄する。
    TERMINATE,  // EOF
} Status ;

#endif

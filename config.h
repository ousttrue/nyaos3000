#ifndef CONFIG_H
#define CONFIG_H

#if defined(__TURBOC__)  /* Borland製コンパイラ */
#  if defined(__BORLANDC__)  /* Borland C++ */
#    define NYACUS                      /* NYACUS を作成する */
#    undef  ESCAPE_SEQUENCE_OK          /* エスケープシーケンスが解釈不能 */
#    undef  ARRAY_DELETE_NEED_SIZE      /* 配列のdeleteに要素数は不要 */
#    define TEMPLATE_OK
#  else                      /* Turbo C++ */
#    define NYADOS                      /* NYADOS を作成する */
#    define ESCAPE_SEQUENCE_OK          /* エスケープシーケンスを解釈可能 */
#    define ARRAY_DELETE_NEED_SIZE      /* 配列のdeleteに要素数が必要 */
#    define TEMPLATE_NG
#  endif
#elif defined(__DMC__)  /* DigitalMars C++ */
#   if defined(_MSDOS)
#     define  NYADOS
#     define  ESCAPE_SEQUENCE_OK        /* エスケープシーケンスが解釈可能 */
#     define  ASM_OK
#   elif defined(__OS2__)
#     define  OS2DMX
#     define  NYAOS2
#     define  ESCAPE_SEQUENCE_OK        /* エスケープシーケンスが解釈可能 */
#   else
#     define  NYACUS
#     undef   ESCAPE_SEQUENCE_OK        /* エスケープシーケンスが解釈不能 */
#   endif
#   undef   ARRAY_DELETE_NEED_SIZE      /* 配列のdeleteに要素数は不要 */
#   define  TEMPLATE_OK
#elif defined(_MSC_VER)					/* VC */
#    define NYACUS                      /* NYACUS を作成する */
#    undef  ESCAPE_SEQUENCE_OK          /* エスケープシーケンスが解釈不能 */
#    undef  ARRAY_DELETE_NEED_SIZE      /* 配列のdeleteに要素数は不要 */
#    define TEMPLATE_OK
      /* _関数の置き換え */
#    include <direct.h>
#	define popen	_popen
#	define pclose	_pclose
#	define chdir	_chdir
#	define setdisk(d)	_chdrive(d+1)
#else                   /* emx/gcc */
#   define  NYAOS2
#   define  ESCAPE_SEQUENCE_OK          /* エスケープシーケンスが解釈可能 */
#   undef   ARRAY_DELETE_NEED_SIZE      /* 配列のdeleteに要素数は不要 */
#   define TEMPLATE_NG  /* 本当はテンプレート Ok だが、テストができないので */
#endif

#if defined(__LARGE__) || defined(__COMPACT__)   /* TC++,DMC++共用 */
#  define   USE_FAR_PTR 1                /* FARポインターを使う */
#endif

#define COMPACT_LEVEL 0

#if defined(NYADOS)
#  define SHELL_NAME		"NYADOS"
#elif defined(NYACUS)
#  define SHELL_NAME		"NYACUS"
#elif defined(NYAOS2)
#  define SHELL_NAME		"NYAOS2"
#else
#  error NO SUPPORT COMPILER
#endif

#define RUN_COMMANDS "_nya"


#endif

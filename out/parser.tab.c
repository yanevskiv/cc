/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 15 "lex/parser.y"

#include <stdio.h>
#include <stdlib.h>
#include "cc.h"

int  yylex(void);
extern int yylineno;
void yyerror(const char *s);

/* State for the function definition currently being parsed. */
static char    *cur_func_name;
static Ast_Var *cur_params;
static Ast_Var *cur_params_tail;
static int      cur_nparams;

/* The program is assembled here as functions are reduced. */
static Ast_Function *prog_head, *prog_tail;

static void add_param(Ast_Var *v)
{
    v->param_next = NULL;
    if (!cur_params) cur_params = cur_params_tail = v;
    else { cur_params_tail->param_next = v; cur_params_tail = v; }
    cur_nparams++;
}

static void add_function(Ast_Function *fn)
{
    fn->next = NULL;
    if (!prog_head) prog_head = prog_tail = fn;
    else { prog_tail->next = fn; prog_tail = fn; }
    Ast_Program = prog_head;
}

#line 106 "out/parser.tab.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "parser.tab.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_NUM = 3,                        /* NUM  */
  YYSYMBOL_IDENT = 4,                      /* IDENT  */
  YYSYMBOL_STR = 5,                        /* STR  */
  YYSYMBOL_INT = 6,                        /* INT  */
  YYSYMBOL_CHAR = 7,                       /* CHAR  */
  YYSYMBOL_VOID = 8,                       /* VOID  */
  YYSYMBOL_CONST = 9,                      /* CONST  */
  YYSYMBOL_RETURN = 10,                    /* RETURN  */
  YYSYMBOL_IF = 11,                        /* IF  */
  YYSYMBOL_ELSE = 12,                      /* ELSE  */
  YYSYMBOL_FOR = 13,                       /* FOR  */
  YYSYMBOL_WHILE = 14,                     /* WHILE  */
  YYSYMBOL_BREAK = 15,                     /* BREAK  */
  YYSYMBOL_CONTINUE = 16,                  /* CONTINUE  */
  YYSYMBOL_EQ = 17,                        /* EQ  */
  YYSYMBOL_NE = 18,                        /* NE  */
  YYSYMBOL_LE = 19,                        /* LE  */
  YYSYMBOL_GE = 20,                        /* GE  */
  YYSYMBOL_AND = 21,                       /* AND  */
  YYSYMBOL_OR = 22,                        /* OR  */
  YYSYMBOL_ELLIPSIS = 23,                  /* ELLIPSIS  */
  YYSYMBOL_LOWER_THAN_ELSE = 24,           /* LOWER_THAN_ELSE  */
  YYSYMBOL_25_ = 25,                       /* '='  */
  YYSYMBOL_26_ = 26,                       /* '<'  */
  YYSYMBOL_27_ = 27,                       /* '>'  */
  YYSYMBOL_28_ = 28,                       /* '+'  */
  YYSYMBOL_29_ = 29,                       /* '-'  */
  YYSYMBOL_30_ = 30,                       /* '*'  */
  YYSYMBOL_31_ = 31,                       /* '/'  */
  YYSYMBOL_32_ = 32,                       /* '%'  */
  YYSYMBOL_33_ = 33,                       /* '!'  */
  YYSYMBOL_UMINUS = 34,                    /* UMINUS  */
  YYSYMBOL_35_ = 35,                       /* '('  */
  YYSYMBOL_36_ = 36,                       /* ')'  */
  YYSYMBOL_37_ = 37,                       /* ';'  */
  YYSYMBOL_38_ = 38,                       /* ','  */
  YYSYMBOL_39_ = 39,                       /* '{'  */
  YYSYMBOL_40_ = 40,                       /* '}'  */
  YYSYMBOL_YYACCEPT = 41,                  /* $accept  */
  YYSYMBOL_translation_unit = 42,          /* translation_unit  */
  YYSYMBOL_external_decl = 43,             /* external_decl  */
  YYSYMBOL_44_1 = 44,                      /* $@1  */
  YYSYMBOL_func_tail = 45,                 /* func_tail  */
  YYSYMBOL_params = 46,                    /* params  */
  YYSYMBOL_param_list = 47,                /* param_list  */
  YYSYMBOL_param = 48,                     /* param  */
  YYSYMBOL_type_name = 49,                 /* type_name  */
  YYSYMBOL_quals = 50,                     /* quals  */
  YYSYMBOL_base = 51,                      /* base  */
  YYSYMBOL_stars = 52,                     /* stars  */
  YYSYMBOL_compound_stmt = 53,             /* compound_stmt  */
  YYSYMBOL_stmt_list = 54,                 /* stmt_list  */
  YYSYMBOL_stmt = 55,                      /* stmt  */
  YYSYMBOL_decl = 56,                      /* decl  */
  YYSYMBOL_expr_opt = 57,                  /* expr_opt  */
  YYSYMBOL_expr = 58,                      /* expr  */
  YYSYMBOL_args = 59,                      /* args  */
  YYSYMBOL_arg_list = 60                   /* arg_list  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  2
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   288

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  41
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  20
/* YYNRULES -- Number of rules.  */
#define YYNRULES  64
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  114

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   280


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    33,     2,     2,     2,    32,     2,     2,
      35,    36,    30,    28,    38,    29,     2,    31,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    37,
      26,    25,    27,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    39,     2,    40,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      34
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint8 yyrline[] =
{
       0,    82,    82,    83,    88,    87,    99,   109,   113,   114,
     118,   119,   123,   124,   125,   131,   135,   136,   140,   141,
     142,   146,   147,   153,   158,   159,   163,   164,   165,   167,
     169,   172,   174,   175,   176,   177,   181,   183,   190,   191,
     197,   198,   200,   204,   206,   207,   208,   209,   210,   211,
     212,   213,   214,   215,   216,   217,   218,   219,   220,   223,
     224,   228,   229,   233,   234
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "NUM", "IDENT", "STR",
  "INT", "CHAR", "VOID", "CONST", "RETURN", "IF", "ELSE", "FOR", "WHILE",
  "BREAK", "CONTINUE", "EQ", "NE", "LE", "GE", "AND", "OR", "ELLIPSIS",
  "LOWER_THAN_ELSE", "'='", "'<'", "'>'", "'+'", "'-'", "'*'", "'/'",
  "'%'", "'!'", "UMINUS", "'('", "')'", "';'", "','", "'{'", "'}'",
  "$accept", "translation_unit", "external_decl", "$@1", "func_tail",
  "params", "param_list", "param", "type_name", "quals", "base", "stars",
  "compound_stmt", "stmt_list", "stmt", "decl", "expr_opt", "expr", "args",
  "arg_list", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-101)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-25)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -101,     7,  -101,  -101,    21,     4,     6,  -101,  -101,  -101,
    -101,  -101,  -101,   -22,   -20,  -101,  -101,     9,     8,  -101,
      43,   -13,    25,  -101,  -101,    39,  -101,  -101,  -101,  -101,
      28,  -101,    54,    32,    35,    36,    89,    89,    89,  -101,
      56,  -101,    42,    39,    64,   108,    89,  -101,   129,    89,
      89,    89,  -101,  -101,   150,    77,  -101,  -101,  -101,    89,
      89,    89,    89,    89,    89,    89,    89,    89,    89,    89,
      89,    89,    89,  -101,    78,    81,  -101,  -101,   170,    82,
     210,   190,  -101,    89,   256,   256,    83,    83,   242,   226,
     210,    83,    83,   -10,   -10,  -101,  -101,  -101,    89,  -101,
      51,    89,    51,   210,  -101,   109,    86,  -101,    51,    89,
    -101,    84,    51,  -101
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] =
{
       2,    16,     1,     3,     0,     0,     0,    18,    19,    20,
      17,    21,     4,    15,    16,    22,    14,     0,     9,    10,
      13,     0,    16,    12,     7,    16,     5,     6,    11,    40,
      42,    41,     0,     0,     0,     0,     0,     0,     0,    35,
       0,    32,     0,    16,     0,     0,    61,    27,     0,     0,
      38,     0,    59,    60,     0,    36,    23,    25,    33,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    34,    63,     0,    62,    26,     0,     0,
      39,     0,    44,     0,    50,    51,    54,    55,    56,    57,
      58,    52,    53,    45,    46,    47,    48,    49,     0,    43,
      16,    38,    16,    37,    64,    28,     0,    31,    16,    38,
      29,     0,    16,    30
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
    -101,  -101,  -101,  -101,  -101,  -101,  -101,   110,     1,  -101,
    -101,  -101,   120,    88,   -27,  -101,  -100,   -32,  -101,    44
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
       0,     1,     3,    14,    26,    17,    18,    19,    40,     5,
      11,    13,    41,    42,    43,    44,    79,    45,    75,    76
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int8 yytable[] =
{
      48,   106,     4,    16,    52,    53,    54,     2,    15,   111,
       7,     8,     9,    10,    74,    20,    -8,    78,    80,    81,
      70,    71,    72,    20,    24,     6,    25,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      97,    12,    29,    30,    31,    21,    22,    23,    16,    32,
      33,   103,    34,    35,    29,    30,    31,    29,    30,    31,
      55,    32,    33,    46,    34,    35,    74,    49,    36,    80,
      50,    51,    37,   105,    38,   107,    39,    80,    25,   -24,
      36,   110,    56,    36,    37,   113,    38,    37,    39,    38,
      25,    47,    29,    30,    31,    59,    60,    61,    62,    63,
      64,    58,    83,    65,    66,    67,    68,    69,    70,    71,
      72,    68,    69,    70,    71,    72,    98,    99,    36,   101,
     112,   108,    37,   109,    38,    59,    60,    61,    62,    63,
      64,    57,    28,    65,    66,    67,    68,    69,    70,    71,
      72,    27,   104,     0,     0,    73,    59,    60,    61,    62,
      63,    64,     0,     0,    65,    66,    67,    68,    69,    70,
      71,    72,     0,     0,     0,     0,    77,    59,    60,    61,
      62,    63,    64,     0,     0,    65,    66,    67,    68,    69,
      70,    71,    72,     0,     0,     0,    82,    59,    60,    61,
      62,    63,    64,     0,     0,    65,    66,    67,    68,    69,
      70,    71,    72,     0,     0,     0,   100,    59,    60,    61,
      62,    63,    64,     0,     0,    65,    66,    67,    68,    69,
      70,    71,    72,     0,     0,     0,   102,    59,    60,    61,
      62,    63,    64,     0,     0,    65,    66,    67,    68,    69,
      70,    71,    72,    59,    60,    61,    62,    63,     0,     0,
       0,     0,    66,    67,    68,    69,    70,    71,    72,    59,
      60,    61,    62,     0,     0,     0,     0,     0,    66,    67,
      68,    69,    70,    71,    72,    61,    62,     0,     0,     0,
       0,     0,    66,    67,    68,    69,    70,    71,    72
};

static const yytype_int8 yycheck[] =
{
      32,   101,     1,    23,    36,    37,    38,     0,    30,   109,
       6,     7,     8,     9,    46,    14,    36,    49,    50,    51,
      30,    31,    32,    22,    37,     4,    39,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    35,     3,     4,     5,    36,    38,     4,    23,    10,
      11,    83,    13,    14,     3,     4,     5,     3,     4,     5,
       4,    10,    11,    35,    13,    14,    98,    35,    29,   101,
      35,    35,    33,   100,    35,   102,    37,   109,    39,    40,
      29,   108,    40,    29,    33,   112,    35,    33,    37,    35,
      39,    37,     3,     4,     5,    17,    18,    19,    20,    21,
      22,    37,    25,    25,    26,    27,    28,    29,    30,    31,
      32,    28,    29,    30,    31,    32,    38,    36,    29,    37,
      36,    12,    33,    37,    35,    17,    18,    19,    20,    21,
      22,    43,    22,    25,    26,    27,    28,    29,    30,    31,
      32,    21,    98,    -1,    -1,    37,    17,    18,    19,    20,
      21,    22,    -1,    -1,    25,    26,    27,    28,    29,    30,
      31,    32,    -1,    -1,    -1,    -1,    37,    17,    18,    19,
      20,    21,    22,    -1,    -1,    25,    26,    27,    28,    29,
      30,    31,    32,    -1,    -1,    -1,    36,    17,    18,    19,
      20,    21,    22,    -1,    -1,    25,    26,    27,    28,    29,
      30,    31,    32,    -1,    -1,    -1,    36,    17,    18,    19,
      20,    21,    22,    -1,    -1,    25,    26,    27,    28,    29,
      30,    31,    32,    -1,    -1,    -1,    36,    17,    18,    19,
      20,    21,    22,    -1,    -1,    25,    26,    27,    28,    29,
      30,    31,    32,    17,    18,    19,    20,    21,    -1,    -1,
      -1,    -1,    26,    27,    28,    29,    30,    31,    32,    17,
      18,    19,    20,    -1,    -1,    -1,    -1,    -1,    26,    27,
      28,    29,    30,    31,    32,    19,    20,    -1,    -1,    -1,
      -1,    -1,    26,    27,    28,    29,    30,    31,    32
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,    42,     0,    43,    49,    50,     4,     6,     7,     8,
       9,    51,    35,    52,    44,    30,    23,    46,    47,    48,
      49,    36,    38,     4,    37,    39,    45,    53,    48,     3,
       4,     5,    10,    11,    13,    14,    29,    33,    35,    37,
      49,    53,    54,    55,    56,    58,    35,    37,    58,    35,
      35,    35,    58,    58,    58,     4,    40,    54,    37,    17,
      18,    19,    20,    21,    22,    25,    26,    27,    28,    29,
      30,    31,    32,    37,    58,    59,    60,    37,    58,    57,
      58,    58,    36,    25,    58,    58,    58,    58,    58,    58,
      58,    58,    58,    58,    58,    58,    58,    58,    38,    36,
      36,    37,    36,    58,    60,    55,    57,    55,    12,    37,
      55,    57,    36,    55
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    41,    42,    42,    44,    43,    45,    45,    46,    46,
      47,    47,    48,    48,    48,    49,    50,    50,    51,    51,
      51,    52,    52,    53,    54,    54,    55,    55,    55,    55,
      55,    55,    55,    55,    55,    55,    56,    56,    57,    57,
      58,    58,    58,    58,    58,    58,    58,    58,    58,    58,
      58,    58,    58,    58,    58,    58,    58,    58,    58,    58,
      58,    59,    59,    60,    60
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     0,     2,     0,     7,     1,     1,     0,     1,
       1,     3,     2,     1,     1,     3,     0,     2,     1,     1,
       1,     0,     2,     3,     0,     2,     3,     2,     5,     7,
       9,     5,     1,     2,     2,     1,     2,     4,     0,     1,
       1,     1,     1,     4,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     2,
       2,     0,     1,     1,     3
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)]);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 4: /* $@1: %empty  */
#line 88 "lex/parser.y"
        {
            cur_func_name   = (yyvsp[-1].str);
            cur_params      = NULL;
            cur_params_tail = NULL;
            cur_nparams     = 0;
            Ast_BeginScope();
        }
#line 1264 "out/parser.tab.c"
    break;

  case 6: /* func_tail: compound_stmt  */
#line 100 "lex/parser.y"
        {
            Ast_Function *fn = calloc(1, sizeof(Ast_Function));
            fn->name      = cur_func_name;
            fn->body      = (yyvsp[0].node);
            fn->params    = cur_params;
            fn->nparams   = cur_nparams;
            fn->locals    = Ast_CurrentLocals();
            add_function(fn);
        }
#line 1278 "out/parser.tab.c"
    break;

  case 12: /* param: type_name IDENT  */
#line 123 "lex/parser.y"
                        { add_param(Ast_DeclareVar((yyvsp[0].str))); }
#line 1284 "out/parser.tab.c"
    break;

  case 23: /* compound_stmt: '{' stmt_list '}'  */
#line 154 "lex/parser.y"
        { Ast_Node *n = Ast_NewNode(ND_BLOCK); n->body = (yyvsp[-1].node); (yyval.node) = n; }
#line 1290 "out/parser.tab.c"
    break;

  case 24: /* stmt_list: %empty  */
#line 158 "lex/parser.y"
                           { (yyval.node) = NULL; }
#line 1296 "out/parser.tab.c"
    break;

  case 25: /* stmt_list: stmt stmt_list  */
#line 159 "lex/parser.y"
                           { (yyvsp[-1].node)->next = (yyvsp[0].node); (yyval.node) = (yyvsp[-1].node); }
#line 1302 "out/parser.tab.c"
    break;

  case 26: /* stmt: RETURN expr ';'  */
#line 163 "lex/parser.y"
                           { (yyval.node) = Ast_NewUnary(ND_RETURN, (yyvsp[-1].node)); }
#line 1308 "out/parser.tab.c"
    break;

  case 27: /* stmt: RETURN ';'  */
#line 164 "lex/parser.y"
                           { (yyval.node) = Ast_NewUnary(ND_RETURN, NULL); }
#line 1314 "out/parser.tab.c"
    break;

  case 28: /* stmt: IF '(' expr ')' stmt  */
#line 166 "lex/parser.y"
        { Ast_Node *n = Ast_NewNode(ND_IF); n->cond = (yyvsp[-2].node); n->then = (yyvsp[0].node); (yyval.node) = n; }
#line 1320 "out/parser.tab.c"
    break;

  case 29: /* stmt: IF '(' expr ')' stmt ELSE stmt  */
#line 168 "lex/parser.y"
        { Ast_Node *n = Ast_NewNode(ND_IF); n->cond = (yyvsp[-4].node); n->then = (yyvsp[-2].node); n->els = (yyvsp[0].node); (yyval.node) = n; }
#line 1326 "out/parser.tab.c"
    break;

  case 30: /* stmt: FOR '(' expr_opt ';' expr_opt ';' expr_opt ')' stmt  */
#line 170 "lex/parser.y"
        { Ast_Node *n = Ast_NewNode(ND_FOR);
          n->init = (yyvsp[-6].node); n->cond = (yyvsp[-4].node); n->inc = (yyvsp[-2].node); n->body = (yyvsp[0].node); (yyval.node) = n; }
#line 1333 "out/parser.tab.c"
    break;

  case 31: /* stmt: WHILE '(' expr ')' stmt  */
#line 173 "lex/parser.y"
        { Ast_Node *n = Ast_NewNode(ND_FOR); n->cond = (yyvsp[-2].node); n->body = (yyvsp[0].node); (yyval.node) = n; }
#line 1339 "out/parser.tab.c"
    break;

  case 32: /* stmt: compound_stmt  */
#line 174 "lex/parser.y"
                           { (yyval.node) = (yyvsp[0].node); }
#line 1345 "out/parser.tab.c"
    break;

  case 33: /* stmt: decl ';'  */
#line 175 "lex/parser.y"
                           { (yyval.node) = (yyvsp[-1].node); }
#line 1351 "out/parser.tab.c"
    break;

  case 34: /* stmt: expr ';'  */
#line 176 "lex/parser.y"
                           { (yyval.node) = Ast_NewUnary(ND_EXPR_STMT, (yyvsp[-1].node)); }
#line 1357 "out/parser.tab.c"
    break;

  case 35: /* stmt: ';'  */
#line 177 "lex/parser.y"
                           { (yyval.node) = Ast_NewNode(ND_NOP); }
#line 1363 "out/parser.tab.c"
    break;

  case 36: /* decl: type_name IDENT  */
#line 182 "lex/parser.y"
        { Ast_DeclareVar((yyvsp[0].str)); (yyval.node) = Ast_NewNode(ND_NOP); }
#line 1369 "out/parser.tab.c"
    break;

  case 37: /* decl: type_name IDENT '=' expr  */
#line 184 "lex/parser.y"
        { Ast_Var *v = Ast_DeclareVar((yyvsp[-2].str));
          (yyval.node) = Ast_NewUnary(ND_EXPR_STMT,
                            Ast_NewBinary(ND_ASSIGN, Ast_NewVarNode(v), (yyvsp[0].node))); }
#line 1377 "out/parser.tab.c"
    break;

  case 38: /* expr_opt: %empty  */
#line 190 "lex/parser.y"
                           { (yyval.node) = NULL; }
#line 1383 "out/parser.tab.c"
    break;

  case 39: /* expr_opt: expr  */
#line 191 "lex/parser.y"
                           { (yyval.node) = (yyvsp[0].node); }
#line 1389 "out/parser.tab.c"
    break;

  case 40: /* expr: NUM  */
#line 197 "lex/parser.y"
                           { (yyval.node) = Ast_NewNum((yyvsp[0].num)); }
#line 1395 "out/parser.tab.c"
    break;

  case 41: /* expr: STR  */
#line 198 "lex/parser.y"
                           { Ast_Node *n = Ast_NewNode(ND_STR);
                             n->str_idx = Ast_AddString((yyvsp[0].str)); (yyval.node) = n; }
#line 1402 "out/parser.tab.c"
    break;

  case 42: /* expr: IDENT  */
#line 201 "lex/parser.y"
        { Ast_Var *v = Ast_FindVar((yyvsp[0].str));
          if (!v) error("use of undeclared identifier '%s'", (yyvsp[0].str));
          (yyval.node) = Ast_NewVarNode(v); }
#line 1410 "out/parser.tab.c"
    break;

  case 43: /* expr: IDENT '(' args ')'  */
#line 205 "lex/parser.y"
        { Ast_Node *n = Ast_NewNode(ND_CALL); n->funcname = (yyvsp[-3].str); n->args = (yyvsp[-1].node); (yyval.node) = n; }
#line 1416 "out/parser.tab.c"
    break;

  case 44: /* expr: '(' expr ')'  */
#line 206 "lex/parser.y"
                           { (yyval.node) = (yyvsp[-1].node); }
#line 1422 "out/parser.tab.c"
    break;

  case 45: /* expr: expr '+' expr  */
#line 207 "lex/parser.y"
                           { (yyval.node) = Ast_NewBinary(ND_ADD, (yyvsp[-2].node), (yyvsp[0].node)); }
#line 1428 "out/parser.tab.c"
    break;

  case 46: /* expr: expr '-' expr  */
#line 208 "lex/parser.y"
                           { (yyval.node) = Ast_NewBinary(ND_SUB, (yyvsp[-2].node), (yyvsp[0].node)); }
#line 1434 "out/parser.tab.c"
    break;

  case 47: /* expr: expr '*' expr  */
#line 209 "lex/parser.y"
                           { (yyval.node) = Ast_NewBinary(ND_MUL, (yyvsp[-2].node), (yyvsp[0].node)); }
#line 1440 "out/parser.tab.c"
    break;

  case 48: /* expr: expr '/' expr  */
#line 210 "lex/parser.y"
                           { (yyval.node) = Ast_NewBinary(ND_DIV, (yyvsp[-2].node), (yyvsp[0].node)); }
#line 1446 "out/parser.tab.c"
    break;

  case 49: /* expr: expr '%' expr  */
#line 211 "lex/parser.y"
                           { (yyval.node) = Ast_NewBinary(ND_MOD, (yyvsp[-2].node), (yyvsp[0].node)); }
#line 1452 "out/parser.tab.c"
    break;

  case 50: /* expr: expr EQ expr  */
#line 212 "lex/parser.y"
                           { (yyval.node) = Ast_NewBinary(ND_EQ, (yyvsp[-2].node), (yyvsp[0].node)); }
#line 1458 "out/parser.tab.c"
    break;

  case 51: /* expr: expr NE expr  */
#line 213 "lex/parser.y"
                           { (yyval.node) = Ast_NewBinary(ND_NE, (yyvsp[-2].node), (yyvsp[0].node)); }
#line 1464 "out/parser.tab.c"
    break;

  case 52: /* expr: expr '<' expr  */
#line 214 "lex/parser.y"
                           { (yyval.node) = Ast_NewBinary(ND_LT, (yyvsp[-2].node), (yyvsp[0].node)); }
#line 1470 "out/parser.tab.c"
    break;

  case 53: /* expr: expr '>' expr  */
#line 215 "lex/parser.y"
                           { (yyval.node) = Ast_NewBinary(ND_LT, (yyvsp[0].node), (yyvsp[-2].node)); }
#line 1476 "out/parser.tab.c"
    break;

  case 54: /* expr: expr LE expr  */
#line 216 "lex/parser.y"
                           { (yyval.node) = Ast_NewBinary(ND_LE, (yyvsp[-2].node), (yyvsp[0].node)); }
#line 1482 "out/parser.tab.c"
    break;

  case 55: /* expr: expr GE expr  */
#line 217 "lex/parser.y"
                           { (yyval.node) = Ast_NewBinary(ND_LE, (yyvsp[0].node), (yyvsp[-2].node)); }
#line 1488 "out/parser.tab.c"
    break;

  case 56: /* expr: expr AND expr  */
#line 218 "lex/parser.y"
                           { (yyval.node) = Ast_NewBinary(ND_AND, (yyvsp[-2].node), (yyvsp[0].node)); }
#line 1494 "out/parser.tab.c"
    break;

  case 57: /* expr: expr OR expr  */
#line 219 "lex/parser.y"
                           { (yyval.node) = Ast_NewBinary(ND_OR, (yyvsp[-2].node), (yyvsp[0].node)); }
#line 1500 "out/parser.tab.c"
    break;

  case 58: /* expr: expr '=' expr  */
#line 221 "lex/parser.y"
        { if ((yyvsp[-2].node)->kind != ND_VAR) error("expression is not assignable");
          (yyval.node) = Ast_NewBinary(ND_ASSIGN, (yyvsp[-2].node), (yyvsp[0].node)); }
#line 1507 "out/parser.tab.c"
    break;

  case 59: /* expr: '-' expr  */
#line 223 "lex/parser.y"
                            { (yyval.node) = Ast_NewUnary(ND_NEG, (yyvsp[0].node)); }
#line 1513 "out/parser.tab.c"
    break;

  case 60: /* expr: '!' expr  */
#line 224 "lex/parser.y"
                            { (yyval.node) = Ast_NewUnary(ND_NOT, (yyvsp[0].node)); }
#line 1519 "out/parser.tab.c"
    break;

  case 61: /* args: %empty  */
#line 228 "lex/parser.y"
                           { (yyval.node) = NULL; }
#line 1525 "out/parser.tab.c"
    break;

  case 62: /* args: arg_list  */
#line 229 "lex/parser.y"
                           { (yyval.node) = (yyvsp[0].node); }
#line 1531 "out/parser.tab.c"
    break;

  case 63: /* arg_list: expr  */
#line 233 "lex/parser.y"
                           { (yyval.node) = (yyvsp[0].node); }
#line 1537 "out/parser.tab.c"
    break;

  case 64: /* arg_list: expr ',' arg_list  */
#line 234 "lex/parser.y"
                           { (yyvsp[-2].node)->next = (yyvsp[0].node); (yyval.node) = (yyvsp[-2].node); }
#line 1543 "out/parser.tab.c"
    break;


#line 1547 "out/parser.tab.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 237 "lex/parser.y"


void yyerror(const char *s)
{
    fprintf(stderr, "cc: parse error: %s near line %d\n", s, yylineno);
    exit(1);
}

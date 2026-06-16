/* lexer.flex - token rules for the cc compiler.
 *
 * Generates build/lex.yy.c (via `flex -o build/lex.yy.c`).  Tokens and the
 * semantic-value union are defined by bison in build/parser.tab.h.
 */
%option noyywrap nounput noinput
%option yylineno

%{
#include <stdlib.h>
#include <string.h>
#include "cc.h"
#include "parser.tab.h"

/* Decode a C string/char literal body (without the surrounding quotes),
 * translating the escape sequences we support into raw bytes. */
static char *unescape(const char *p, int len)
{
    char *buf = malloc(len + 1);
    int   n   = 0;
    for (int i = 0; i < len; i++) {
        if (p[i] != '\\') { buf[n++] = p[i]; continue; }
        switch (p[++i]) {
            case 'n':  buf[n++] = '\n'; break;
            case 't':  buf[n++] = '\t'; break;
            case 'r':  buf[n++] = '\r'; break;
            case '0':  buf[n++] = '\0'; break;
            case '\\': buf[n++] = '\\'; break;
            case '\'': buf[n++] = '\''; break;
            case '"':  buf[n++] = '"';  break;
            default:   buf[n++] = p[i]; break;
        }
    }
    buf[n] = '\0';
    return buf;
}
%}

D   [0-9]
L   [A-Za-z_]
A   [A-Za-z_0-9]

%%

[ \t\r\n]+              ;                       /* whitespace          */
"//"[^\n]*              ;                       /* line comment        */
"/*"([^*]|\*+[^*/])*\*+"/"  ;                   /* block comment       */

"int"                   return INT;
"char"                  return CHAR;
"void"                  return VOID;
"const"                 return CONST;
"return"                return RETURN;
"if"                    return IF;
"else"                  return ELSE;
"for"                   return FOR;
"while"                 return WHILE;
"break"                 return BREAK;
"continue"              return CONTINUE;

{L}{A}*                 { yylval.str = strdup(yytext); return IDENT; }

0[xX][0-9A-Fa-f]+       { yylval.num = strtol(yytext, NULL, 16); return NUM; }
{D}+                    { yylval.num = strtol(yytext, NULL, 10); return NUM; }

\"([^"\\\n]|\\.)*\"     { yylval.str = unescape(yytext + 1, yyleng - 2); return STR; }
'([^'\\\n]|\\.)'        { char *s = unescape(yytext + 1, yyleng - 2);
                          yylval.num = (unsigned char)s[0]; free(s); return NUM; }

"=="                    return EQ;
"!="                    return NE;
"<="                    return LE;
">="                    return GE;
"&&"                    return AND;
"||"                    return OR;
"..."                   return ELLIPSIS;

[-+*/%=<>!(){};,&]       return yytext[0];

.                       { error("lexer: unexpected character '%s' on line %d",
                                yytext, yylineno); }

%%

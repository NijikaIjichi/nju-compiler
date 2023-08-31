%option yylineno
%{
#include "ast.h"
#include "syntax.tab.h"
void set_error();
%}
%%
" "|\t|\r|\n {}
"LABEL" { return LABEL; }
"FUNCTION" { return FUNCTION; }
"&" { return ADDR; }
"GOTO" { return GOTO; }
"IF" { return IF; }
"RETURN" { return RETURN; }
"DEC" { return DEC; }
"ARG" { return ARG; }
"CALL" { return CALL; }
"PARAM" { return PARAM; }
"READ" { return READ; }
"WRITE" { return WRITE; }
"*"[a-zA-Z_.][0-9a-zA-Z_.]* { yylval.id = strdup(yytext) + 1; return STARID; }
-?(0|[1-9][0-9]*) { yylval.ival = atoi(yytext); return INT; }
":=" { return ASSIGN; }
">" { yylval.relop = GT; return RELOP; }
"<" { yylval.relop = LT; return RELOP; }
">=" { yylval.relop = GE; return RELOP; }
"<=" { yylval.relop = LE; return RELOP; }
"==" { yylval.relop = EQ; return RELOP; }
"!=" { yylval.relop = NEQ; return RELOP; }
"+" { return PLUS; }
"-" { return MINUS; }
"*" { return STAR; }
"/" { return DIV; }
"#" { return HASH; }
":" { return COLON; }
[a-zA-Z_.][0-9a-zA-Z_.]* { yylval.id = strdup(yytext); return ID; }
. { set_error();
  printf("Error type A at Line %d: Mysterious characters \'%s\'\n", yylineno, yytext);
}
%%

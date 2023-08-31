%option yylineno
%{
#include "ast.h"
#include "syntax.tab.h"

int yycolumn = 1;
static int input();
static int zzinput() {
  int c = input();
  if (c == '\n') yycolumn = 1;
  else yycolumn += 1;
  return c;
}

void set_error();

#define YY_USER_ACTION \
  yylloc.first_line = yylloc.last_line = yylineno; \
  yylloc.first_column = yycolumn; \
  yylloc.last_column = yycolumn + yyleng - 1; \
  yycolumn += yyleng;
%}
%%
" "|\t|\r {}
\n { yycolumn = 1; }
"//" { while (zzinput() != '\n') ; }
"/*" { char c1, c2;
  c1 = zzinput();
  c2 = zzinput();
  while (c1 != '*' || c2 != '/') {
    if (c1 == 0 || c2 == 0) {
      set_error();
      printf("Error type A at Line %d: Unexpexted EOF\n", yylineno);
      break;
    }
    c1 = c2;
    c2 = zzinput();
  }
}
";" { return SEMI; }
"," { return COMMA; }
"=" { return ASSIGNOP; }
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
"&&" { return AND; }
"||" { return OR; }
"." { return DOT; }
"!" { return NOT; }
"int" { yylval.type = E_type_int; return TYPE; }
"float" { yylval.type = E_type_float; return TYPE; }
"(" { return LP; }
")" { return RP; }
"[" { return LB; }
"]" { return RB; }
"{" { return LC; }
"}" { return RC; }
"struct" { return STRUCT; }
"return" { return RETURN; }
"if" { return IF; }
"else" { return ELSE; }
"while" { return WHILE; }
[a-zA-Z_][0-9a-zA-Z_]* { yylval.id = strdup(yytext); return ID; }
[0-9]+"."[0-9]+ { yylval.fval = atof(yytext); return FLOAT; }
0|[1-9][0-9]* { yylval.ival = atoi(yytext); return INT; }
. { set_error();
  printf("Error type A at Line %d: Mysterious characters \'%s\'\n", yylineno, yytext);
}
%%

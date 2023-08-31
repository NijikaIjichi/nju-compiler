%locations
%{
#include "lex.yy.c"
void set_error();
void yyerror(const char* msg) {
  set_error();
  printf("Error type B at Line %d: %s\n", yylineno, msg);
}
%}

/* declared types */
%union {
  int ival;
  float fval;
  typeid_t type;
  relop_t relop;
  char *id;
  void *node;
}

%token <ival> INT
%token <fval> FLOAT
%token <type> TYPE
%token <relop> RELOP
%token <id> ID
%token SEMI COMMA ASSIGNOP PLUS MINUS STAR DIV AND OR DOT NOT LP RP LB RB LC RC STRUCT RETURN IF ELSE WHILE

%right ASSIGNOP
%left OR
%left AND
%left RELOP
%left PLUS MINUS
%left STAR DIV
%right UMINUS NOT
%left LP RP LB RB DOT
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%type <node> Program ExtDefList ExtDef Specifier ExtDecList FunDec CompSt VarDec StructSpecifier OptTag DefList Tag VarList ParamDec StmtList Stmt Exp Def DecList Dec Args

%start Program
%%

Program: ExtDefList { ast_set_root(($$ = ASTNEW(program, $1))); }
| error { $$ = NULL; }
;
ExtDefList: ExtDef ExtDefList { $$ = ASTNEW(ext_def_list, $1, $2); }
| { $$ = NULL; }
| error { $$ = NULL; }
;
ExtDef: Specifier ExtDecList SEMI { $$ = ASTNEW(ext_def__var, $1, $2); }
| Specifier SEMI { $$ = ASTNEW(ext_def__none, $1); }
| Specifier FunDec CompSt { $$ = ASTNEW(ext_def__fun, $1, $2, $3); }
| error { $$ = NULL; }
;
ExtDecList: VarDec { $$ = ASTNEW(ext_dec_list, $1, NULL); }
| VarDec COMMA ExtDecList { $$ = ASTNEW(ext_dec_list, $1, $3); }
| error { $$ = NULL; }
;
Specifier: TYPE { $$ = ASTNEW(specifier__type, $1); }
| StructSpecifier { $$ = ASTNEW(specifier__struct, $1); }
| error { $$ = NULL; }
;
StructSpecifier: STRUCT OptTag LC DefList RC { $$ = ASTNEW(struct_specifier__def, $2, $4); }
| STRUCT Tag { $$ = ASTNEW(struct_specifier__tag, $2); }
| error { $$ = NULL; }
;
OptTag: ID { $$ = ASTNEW(opt_tag, $1); }
| { $$ = NULL; }
| error { $$ = NULL; }
;
Tag: ID { $$ = ASTNEW(tag, $1); }
| error { $$ = NULL; }
;
VarDec: ID { $$ = ASTNEW(var_dec__id, $1); }
| VarDec LB INT RB { $$ = ASTNEW(var_dec__array, $1, $3); }
| error { $$ = NULL; }
;
FunDec: ID LP VarList RP { $$ = ASTNEW(fun_dec, $1, $3); }
| ID LP RP { $$ = ASTNEW(fun_dec, $1, NULL); }
| error { $$ = NULL; }
;
VarList: ParamDec COMMA VarList { $$ = ASTNEW(var_list, $1, $3); }
| ParamDec { $$ = ASTNEW(var_list, $1, NULL); }
| error { $$ = NULL; }
;
ParamDec: Specifier VarDec { $$ = ASTNEW(param_dec, $1, $2); }
| error { $$ = NULL; }
;
CompSt: LC DefList StmtList RC { $$ = ASTNEW(comp_st, $2, $3); }
| error { $$ = NULL; }
;
StmtList: Stmt StmtList { $$ = ASTNEW(stmt_list, $1, $2); }
| { $$ = NULL; }
| error { $$ = NULL; }
;
Stmt: Exp SEMI { $$ = ASTNEW(stmt__exp, $1); }
| CompSt { $$ = ASTNEW(stmt__comp, $1); }
| RETURN Exp SEMI { $$ = ASTNEW(stmt__ret, $2); }
| IF LP Exp RP Stmt %prec LOWER_THAN_ELSE { $$ = ASTNEW(stmt__if, $3, $5); }
| IF LP Exp RP Stmt ELSE Stmt { $$ = ASTNEW(stmt__ifelse, $3, $5, $7); }
| WHILE LP Exp RP Stmt { $$ = ASTNEW(stmt__while, $3, $5); }
| error { $$ = NULL; }
;
DefList: Def DefList { $$ = ASTNEW(def_list, $1, $2); }
| { $$ = NULL; }
| error { $$ = NULL; }
;
Def: Specifier DecList SEMI { $$ = ASTNEW(def, $1, $2); }
| error { $$ = NULL; }
;
DecList: Dec { $$ = ASTNEW(dec_list, $1, NULL); }
| Dec COMMA DecList { $$ = ASTNEW(dec_list, $1, $3); }
| error { $$ = NULL; }
;
Dec: VarDec { $$ = ASTNEW(dec, $1, NULL); }
| VarDec ASSIGNOP Exp { $$ = ASTNEW(dec, $1, $3); }
| error { $$ = NULL; }
;
Exp: Exp ASSIGNOP Exp { $$ = ASTNEW(exp__2op, $1, $3, OP2_ASSIGNOP, 0); }
| Exp AND Exp { $$ = ASTNEW(exp__2op, $1, $3, OP2_AND, 0); }
| Exp OR Exp { $$ = ASTNEW(exp__2op, $1, $3, OP2_OR, 0); }
| Exp RELOP Exp { $$ = ASTNEW(exp__2op, $1, $3, OP2_RELOP, $2); }
| Exp PLUS Exp { $$ = ASTNEW(exp__2op, $1, $3, OP2_PLUS, 0); }
| Exp MINUS Exp { $$ = ASTNEW(exp__2op, $1, $3, OP2_MINUS, 0); }
| Exp STAR Exp { $$ = ASTNEW(exp__2op, $1, $3, OP2_STAR, 0); }
| Exp DIV Exp { $$ = ASTNEW(exp__2op, $1, $3, OP2_DIV, 0); }
| LP Exp RP { $$ = ASTNEW(exp__para, $2); }
| MINUS Exp %prec UMINUS { $$ = ASTNEW(exp__1op, $2, OP1_MINUS); }
| NOT Exp { $$ = ASTNEW(exp__1op, $2, OP1_NOT); }
| ID LP Args RP { $$ = ASTNEW(exp__call, $1, $3); }
| ID LP RP { $$ = ASTNEW(exp__call, $1, NULL); }
| Exp LB Exp RB { $$ = ASTNEW(exp__array, $1, $3); }
| Exp DOT ID { $$ = ASTNEW(exp__dot, $1, $3); }
| ID { $$ = ASTNEW(exp__id, $1); }
| INT { $$ = ASTNEW(exp__int, $1); }
| FLOAT { $$ = ASTNEW(exp__float, $1); }
| error { $$ = NULL; }
;
Args: Exp COMMA Args { $$ = ASTNEW(args, $1, $3); }
| Exp { $$ = ASTNEW(args, $1, NULL); }
| error { $$ = NULL; }
;
%%

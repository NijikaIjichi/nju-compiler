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
  relop_t relop;
  char *id;
  void *node;
}

%token <ival> INT
%token <id> ID STARID
%token <relop> RELOP
%token LABEL FUNCTION ASSIGN PLUS MINUS STAR DIV ADDR GOTO IF RETURN DEC ARG CALL PARAM READ WRITE HASH COLON

%type <node> Program FunctionList Function InstrList Instr Label FuncHead Mov Arth Goto Branch Ret Dec Arg Call Param Read Write Lopr Ropr

%start Program
%%

Program: FunctionList { ast_set_root(($$ = ASTNEW(program, $1))); }
;
FunctionList: Function FunctionList { $$ = ASTNEW(function_list, $1, $2); }
| { $$ = NULL; }
;
Function: FuncHead InstrList { $$ = ASTNEW(function, $1, $2); }
;
InstrList: Instr InstrList { $$ = ASTNEW(instrlist, $1, $2); }
| { $$ = NULL; }
;
Instr: Label
| Mov
| Arth
| Goto
| Branch
| Ret
| Dec
| Arg
| Call
| Param
| Read
| Write
;
Label: LABEL ID COLON { $$ = ASTNEW(instr_label, $2); }
;
FuncHead: FUNCTION ID COLON { $$ = ASTNEW(instr_func, $2); }
;
Mov: Lopr ASSIGN Ropr { $$ = ASTNEW(instr_mov, $1, $3); }
;
Arth: Lopr ASSIGN Ropr PLUS Ropr { $$ = ASTNEW(instr_arth, $1, $3, $5, OP2_PLUS); }
| Lopr ASSIGN Ropr MINUS Ropr { $$ = ASTNEW(instr_arth, $1, $3, $5, OP2_MINUS); }
| Lopr ASSIGN Ropr STAR Ropr { $$ = ASTNEW(instr_arth, $1, $3, $5, OP2_STAR); }
| Lopr ASSIGN Ropr DIV Ropr { $$ = ASTNEW(instr_arth, $1, $3, $5, OP2_DIV); }
;
Goto: GOTO ID { $$ = ASTNEW(instr_goto, $2); }
;
Branch: IF Ropr RELOP Ropr GOTO ID { $$ = ASTNEW(instr_branch, $2, $4, $3, $6); }
;
Ret: RETURN Ropr { $$ = ASTNEW(instr_ret, $2); }
;
Dec: DEC ID INT { $$ = ASTNEW(instr_dec, $2, $3); }
;
Arg: ARG Ropr { $$ = ASTNEW(instr_arg, $2); }
;
Call: Lopr ASSIGN CALL ID { $$ = ASTNEW(instr_call, $1, $4); }
;
Param: PARAM Lopr { $$ = ASTNEW(instr_param, $2); }
;
Read: READ Lopr { $$ = ASTNEW(instr_read, $2); }
;
Write: WRITE Ropr { $$ = ASTNEW(instr_write, $2); }
;
Lopr: ID { $$ = ASTNEW(lopr_var, $1); }
| STARID { $$ = ASTNEW(lopr_deref, $1); }
;
Ropr: ID  { $$ = ASTNEW(ropr_var, $1); }
| STARID { $$ = ASTNEW(ropr_deref, $1); }
| HASH INT { $$ = ASTNEW(ropr_imm, $2); }
| ADDR ID { $$ = ASTNEW(ropr_addr, $2); }
;
%%

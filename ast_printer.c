#include "ast.h"
#include "ast_visitor.h"
#include <stdio.h>

typedef struct ast_printer {
  void **table;
  int index;
} ast_printer_t;

#define PRINT_INDEX for (int i = 0; i < v->index; ++i) putchar(' ');
#define PUT_FINAL(x) {PRINT_INDEX puts(#x);}
#define PUT_FINAL2(x, y, z) {PRINT_INDEX printf(#x ": %" #y "\n", z);}
#define PUT_NAME(x) {PRINT_INDEX printf(#x " (%d)\n", n->lineno);}

DEF_VISIT_FUNC(ast_printer, program) {
  PUT_NAME(Program)
  v->index += 2;
  ast_visit(v, n->ext_def_list);
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, ext_def_list) {
  PUT_NAME(ExtDefList)
  v->index += 2;
  ast_visit(v, n->ext_def);
  ast_visit(v, n->ext_def_list);
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, ext_def__var) {
  PUT_NAME(ExtDef)
  v->index += 2;
  ast_visit(v, n->specifier);
  ast_visit(v, n->ext_dec_list);
  PUT_FINAL(SEMI)
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, ext_def__none) {
  PUT_NAME(ExtDef)
  v->index += 2;
  ast_visit(v, n->specifier);
  PUT_FINAL(SEMI)
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, ext_def__fun) {
  PUT_NAME(ExtDef)
  v->index += 2;
  ast_visit(v, n->specifier);
  ast_visit(v, n->fun_dec);
  ast_visit(v, n->comp_st);
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, ext_dec_list) {
  PUT_NAME(ExtDecList)
  v->index += 2;
  ast_visit(v, n->var_dec);
  if (n->ext_dec_list) {
    PUT_FINAL(COMMA)
    ast_visit(v, n->ext_dec_list);
  }
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, specifier__type) {
  PUT_NAME(Specifier)
  v->index += 2;
  PUT_FINAL2(TYPE, s, ((char *[]){"int", "float"})[n->type])
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, specifier__struct) {
  PUT_NAME(Specifier)
  v->index += 2;
  ast_visit(v, n->struct_specifier);
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, struct_specifier__def) {
  PUT_NAME(StructSpecifier)
  v->index += 2;
  PUT_FINAL(STRUCT)
  ast_visit(v, n->opt_tag);
  PUT_FINAL(LC)
  ast_visit(v, n->def_list);
  PUT_FINAL(RC)
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, struct_specifier__tag) {
  PUT_NAME(StructSpecifier)
  v->index += 2;
  PUT_FINAL(STRUCT)
  ast_visit(v, n->tag);
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, opt_tag) {
  PUT_NAME(OptTag)
  v->index += 2;
  PUT_FINAL2(ID, s, n->id)
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, tag) {
  PUT_NAME(Tag)
  v->index += 2;
  PUT_FINAL2(ID, s, n->id)
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, var_dec__id) {
  PUT_NAME(VarDec)
  v->index += 2;
  PUT_FINAL2(ID, s, n->id)
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, var_dec__array) {
  PUT_NAME(VarDec)
  v->index += 2;
  ast_visit(v, n->var_dec);
  PUT_FINAL(LB)
  PUT_FINAL2(INT, d, n->ival)
  PUT_FINAL(RB)
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, fun_dec) {
  PUT_NAME(FunDec)
  v->index += 2;
  PUT_FINAL2(ID, s, n->id)
  PUT_FINAL(LP)
  ast_visit(v, n->var_list);
  PUT_FINAL(RP)
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, var_list) {
  PUT_NAME(VarList)
  v->index += 2;
  ast_visit(v, n->param_dec);
  if (n->var_list) {
    PUT_FINAL(COMMA)
    ast_visit(v, n->var_list);
  }
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, param_dec) {
  PUT_NAME(ParamDec)
  v->index += 2;
  ast_visit(v, n->specifier);
  ast_visit(v, n->var_dec);
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, comp_st) {
  PUT_NAME(CompSt)
  v->index += 2;
  PUT_FINAL(LC)
  ast_visit(v, n->def_list);
  ast_visit(v, n->stmt_list);
  PUT_FINAL(RC)
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, stmt_list) {
  PUT_NAME(StmtList)
  v->index += 2;
  ast_visit(v, n->stmt);
  ast_visit(v, n->stmt_list);
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, stmt__exp) {
  PUT_NAME(Stmt)
  v->index += 2;
  ast_visit(v, n->exp);
  PUT_FINAL(SEMI)
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, stmt__comp) {
  PUT_NAME(Stmt)
  v->index += 2;
  ast_visit(v, n->comp_st);
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, stmt__ret) {
  PUT_NAME(Stmt)
  v->index += 2;
  PUT_FINAL(RETURN)
  ast_visit(v, n->exp);
  PUT_FINAL(SEMI)
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, stmt__if) {
  PUT_NAME(Stmt)
  v->index += 2;
  PUT_FINAL(IF)
  PUT_FINAL(LP)
  ast_visit(v, n->exp);
  PUT_FINAL(RP)
  ast_visit(v, n->stmt);
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, stmt__ifelse) {
  PUT_NAME(Stmt)
  v->index += 2;
  PUT_FINAL(IF)
  PUT_FINAL(LP)
  ast_visit(v, n->exp);
  PUT_FINAL(RP)
  ast_visit(v, n->ifst);
  PUT_FINAL(ELSE)
  ast_visit(v, n->elsest);
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, stmt__while) {
  PUT_NAME(Stmt)
  v->index += 2;
  PUT_FINAL(WHILE)
  PUT_FINAL(LP)
  ast_visit(v, n->exp);
  PUT_FINAL(RP)
  ast_visit(v, n->stmt);
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, def_list) {
  PUT_NAME(DefList)
  v->index += 2;
  ast_visit(v, n->def);
  ast_visit(v, n->def_list);
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, def) {
  PUT_NAME(Def)
  v->index += 2;
  ast_visit(v, n->specifier);
  ast_visit(v, n->dec_list);
  PUT_FINAL(SEMI)
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, dec_list) {
  PUT_NAME(DecList)
  v->index += 2;
  ast_visit(v, n->dec);
  if (n->dec_list) {
    PUT_FINAL(COMMA)
    ast_visit(v, n->dec_list);
  }
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, dec) {
  PUT_NAME(Dec)
  v->index += 2;
  ast_visit(v, n->var_dec);
  if (n->exp) {
    PUT_FINAL(ASSIGNOP)
    ast_visit(v, n->exp);
  }
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, exp__2op) {
  PUT_NAME(Exp)
  v->index += 2;
  ast_visit(v, n->lexp);
  PRINT_INDEX puts(((char *[]){"ASSIGNOP", "AND", "OR", "RELOP", "PLUS", "MINUS", "STAR", "DIV"})[n->op]);
  ast_visit(v, n->rexp);
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, exp__para) {
  PUT_NAME(Exp)
  v->index += 2;
  PUT_FINAL(LP)
  ast_visit(v, n->exp);
  PUT_FINAL(RP)
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, exp__1op) {
  PUT_NAME(Exp)
  v->index += 2;
  PRINT_INDEX puts(((char *[]){"MINUS", "NOT"})[n->op]);
  ast_visit(v, n->exp);
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, exp__call) {
  PUT_NAME(Exp)
  v->index += 2;
  PUT_FINAL2(ID, s, n->id)
  PUT_FINAL(LP)
  ast_visit(v, n->args);
  PUT_FINAL(RP)
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, exp__array) {
  PUT_NAME(Exp)
  v->index += 2;
  ast_visit(v, n->arrexp);
  PUT_FINAL(LB)
  ast_visit(v, n->idxexp);
  PUT_FINAL(RB)
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, exp__dot) {
  PUT_NAME(Exp)
  v->index += 2;
  ast_visit(v, n->exp);
  PUT_FINAL(DOT)
  PUT_FINAL2(ID, s, n->id)
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, exp__id) {
  PUT_NAME(Exp)
  v->index += 2;
  PUT_FINAL2(ID, s, n->id)
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, exp__int) {
  PUT_NAME(Exp)
  v->index += 2;
  PUT_FINAL2(INT, u, n->ival)
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, exp__float) {
  PUT_NAME(Exp)
  v->index += 2;
  PUT_FINAL2(FLOAT, f, n->fval)
  v->index -= 2;
  return NULL;
}

DEF_VISIT_FUNC(ast_printer, args) {
  PUT_NAME(Args)
  v->index += 2;
  ast_visit(v, n->exp);
  if (n->args) {
    PUT_FINAL(COMMA)
    ast_visit(v, n->args);
  }
  v->index -= 2;
  return NULL;
}

#define AST_PRINT_FUNC(name, ...) ast_printer_##name,

static ast_visitor_table_t ast_printer_table = {
  ASTALL(AST_PRINT_FUNC)
};

void ast_print() {
  ast_printer_t ast_printer = {ast_printer_table, 0};
  ast_visit(&ast_printer, ast_get_root());
}

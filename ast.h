#ifndef __AST_H__
#define __AST_H__

#include "type.h"
#include "visitor.h"

#define ASTALL(_) \
  _(program, void *ext_def_list) \
  _(ext_def_list, void *ext_def, *ext_def_list) \
  _(ext_def__var, void *specifier, *ext_dec_list) \
  _(ext_def__none, void *specifier) \
  _(ext_def__fun, void *specifier, *fun_dec, *comp_st) \
  _(ext_dec_list, void *var_dec, *ext_dec_list) \
  _(specifier__type, typeid_t type) \
  _(specifier__struct, void *struct_specifier) \
  _(struct_specifier__def, void *opt_tag, *def_list) \
  _(struct_specifier__tag, void *tag) \
  _(opt_tag, char *id) \
  _(tag, char *id) \
  _(var_dec__id, char *id) \
  _(var_dec__array, void *var_dec; int ival) \
  _(fun_dec, char *id; void *var_list) \
  _(var_list, void *param_dec, *var_list) \
  _(param_dec, void *specifier, *var_dec) \
  _(comp_st, void *def_list, *stmt_list) \
  _(stmt_list, void *stmt, *stmt_list) \
  _(stmt__exp, void *exp) \
  _(stmt__comp, void *comp_st) \
  _(stmt__ret, void *exp) \
  _(stmt__if, void *exp, *stmt) \
  _(stmt__ifelse, void *exp, *ifst, *elsest) \
  _(stmt__while, void *exp, *stmt) \
  _(def_list, void *def, *def_list) \
  _(def, void *specifier, *dec_list) \
  _(dec_list, void *dec, *dec_list) \
  _(dec, void *var_dec, *exp) \
  _(exp__2op, void *lexp, *rexp; op2_t op; relop_t relop) \
  _(exp__para, void *exp) \
  _(exp__1op, void *exp; op1_t op) \
  _(exp__call, char *id; void *args) \
  _(exp__array, void *arrexp, *idxexp) \
  _(exp__dot, void *exp; char *id) \
  _(exp__id, char *id) \
  _(exp__int, int ival) \
  _(exp__float, float fval) \
  _(args, void *exp, *args)

typedef enum { ASTALL(DEF_ENUM) E_ASTNUM } astid_t;

#define AST_DEF_STRUCT(name, ...) \
  typedef struct name { \
    astid_t astid; \
    int lineno; \
    void *scratch; \
    __VA_ARGS__; \
  } name##_t;

AST_DEF_STRUCT(abstract_node)
ASTALL(AST_DEF_STRUCT)

#define ASTNEW(name, ...) NEW(name, E_##name, (yyloc).first_line, NULL, ##__VA_ARGS__)

typedef void *ast_visitor_table_t[E_ASTNUM];
void *ast_visit(void *visitor, void *node);

void ast_set_root(program_t *node);
program_t *ast_get_root();

#endif

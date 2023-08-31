#ifndef __AST_H__
#define __AST_H__

#include "type.h"
#include "visitor.h"

#define ASTALL(_) \
  _(program, void *function_list) \
  _(function_list, void *function, *function_list) \
  _(function, void *funchead, *instrlist) \
  _(instrlist, void *instr, *instrlist) \
  _(instr_label, char *id) \
  _(instr_func, char *id) \
  _(instr_mov, void *lopr, *ropr) \
  _(instr_arth, void *lopr, *ropr1, *ropr2; op2_t op) \
  _(instr_goto, char *id) \
  _(instr_branch, void *ropr1, *ropr2; relop_t relop; char *id) \
  _(instr_ret, void *ropr) \
  _(instr_dec, char *id; int size) \
  _(instr_arg, void *ropr) \
  _(instr_call, void *lopr; char *id) \
  _(instr_param, void *lopr) \
  _(instr_read, void *lopr) \
  _(instr_write, void *ropr) \
  _(lopr_var, char *id) \
  _(lopr_deref, char *id) \
  _(ropr_var, char *id) \
  _(ropr_deref, char *id) \
  _(ropr_imm, int imm) \
  _(ropr_addr, char *id)

typedef enum { ASTALL(DEF_ENUM) E_ASTNUM } astid_t;

#define AST_DEF_STRUCT(name, ...) \
  typedef struct name { \
    astid_t astid; \
    __VA_ARGS__; \
  } name##_t;

AST_DEF_STRUCT(abstract_node)
ASTALL(AST_DEF_STRUCT)

#define ASTNEW(name, ...) NEW(name, E_##name, ##__VA_ARGS__)

typedef void *ast_visitor_table_t[E_ASTNUM];
void *ast_visit(void *visitor, void *node);

void ast_set_root(program_t *node);
program_t *ast_get_root();

#endif

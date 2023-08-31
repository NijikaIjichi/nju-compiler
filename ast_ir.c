#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ir.h"
#include "ast.h"
#include "ast_visitor.h"

iropr_imm_t IMM0 = {E_iropr_imm, 0}, IMM1 = {E_iropr_imm, 1};

typedef struct ast_ir {
  void **table;
  HMAP(char *, iropr_var_t *) *var_map;
  HMAP(char *, ir_label_t *) *label_map;
  iroprs_t *args_stack;
  ir_func_t *curr_func;
} ast_ir_t;

DEF_VISIT_FUNC(ast_ir, program) {
  ast_visit(v, n->function_list);
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, function_list) {
  ast_visit(v, n->function);
  ast_visit(v, n->function_list);
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, function) {
  ast_visit(v, n->funchead);
  ast_visit(v, n->instrlist);
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, instrlist) {
  ast_visit(v, n->instr);
  ast_visit(v, n->instrlist);
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, instr_label) {
  ir_label_t *label = hmap_get(v->label_map, n->id);
  if (!label) {
    label = gen_label();
    hmap_put(v->label_map, n->id, label);
  }
  add_ir(label);
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, instr_func) {
  ir_func_t *func = IRNEW(ir_func, n->id, NULL);
  v->curr_func = func;
  add_cfg(func);
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, instr_mov) {
  iropr_var_t *lhs = ast_visit(v, n->lopr);
  iropr_t *rhs = ast_visit(v, n->ropr);
  if (lhs->type->typeid == E_type_ref) {
    add_ir(IRNEW(ir_store, lhs, rhs));
  } else {
    add_ir(IRNEW(ir_mov, lhs, rhs));
  }
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, instr_arth) {
  iropr_var_t *lhs = ast_visit(v, n->lopr);
  iropr_t *rhs1 = ast_visit(v, n->ropr1), *rhs2 = ast_visit(v, n->ropr2);
  if (lhs->type->typeid == E_type_ref) {
    iropr_var_t *tmp = gen_temp_var(&INT);
    add_ir(IRNEW(ir_arth, tmp, rhs1, rhs2, n->op));
    add_ir(IRNEW(ir_store, lhs, (iropr_t *)tmp));
  } else {
    add_ir(IRNEW(ir_arth, lhs, rhs1, rhs2, n->op));
  }
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, instr_goto) {
  ir_label_t *label = hmap_get(v->label_map, n->id);
  if (!label) {
    label = gen_label();
    hmap_put(v->label_map, n->id, label);
  }
  add_ir(IRNEW(ir_goto, label));
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, instr_branch) {
  ir_label_t *label = hmap_get(v->label_map, n->id);
  if (!label) {
    label = gen_label();
    hmap_put(v->label_map, n->id, label);
  }
  iropr_t *rhs1 = ast_visit(v, n->ropr1), *rhs2 = ast_visit(v, n->ropr2);
  add_ir(IRNEW(ir_branch, rhs1, rhs2, n->relop, label));
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, instr_ret) {
  iropr_t *rhs = ast_visit(v, n->ropr);
  add_ir(IRNEW(ir_ret, rhs));
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, instr_dec) {
  iropr_var_t *tmp1 = gen_temp_var(&INT), *tmp2 = gen_temp_var(&INT);
  add_ir(IRNEW(ir_alloc, tmp1, n->size));
  add_ir(IRNEW(ir_addr, tmp2, tmp1));
  hmap_put(v->var_map, n->id, tmp2);
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, instr_arg) {
  iropr_t *rhs = ast_visit(v, n->ropr);
  v->args_stack = NEW(iroprs, rhs, v->args_stack);
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, instr_call) {
  iropr_var_t *lhs = ast_visit(v, n->lopr);
  if (lhs->type->typeid == E_type_ref) {
    iropr_var_t *tmp = gen_temp_var(&INT);
    add_ir(IRNEW(ir_call, tmp, n->id, v->args_stack));
    add_ir(IRNEW(ir_store, lhs, (iropr_t *)tmp));
  } else {
    add_ir(IRNEW(ir_call, lhs, n->id, v->args_stack));
  }
  v->args_stack = NULL;
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, instr_param) {
  iropr_var_t *lhs = ast_visit(v, n->lopr);
  if (lhs->type->typeid == E_type_ref) {
    iropr_var_t *tmp = gen_temp_var(&INT);
    add_ir(IRNEW(ir_store, lhs, (iropr_t *)tmp));
    lhs = tmp;
  }
  iropr_vars_t **pos = &(v->curr_func->params);
  while (*pos) pos = &((*pos)->next);
  *pos = NEW(iropr_vars, lhs, NULL);
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, instr_read) {
  iropr_var_t *lhs = ast_visit(v, n->lopr);
  if (lhs->type->typeid == E_type_ref) {
    iropr_var_t *tmp = gen_temp_var(&INT);
    add_ir(IRNEW(ir_read, tmp));
    add_ir(IRNEW(ir_store, lhs, (iropr_t *)tmp));
  } else {
    add_ir(IRNEW(ir_read, lhs));
  }
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, instr_write) {
  iropr_t *rhs = ast_visit(v, n->ropr);
  add_ir(IRNEW(ir_write, rhs));
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, lopr_var) {
  iropr_var_t *var = hmap_get(v->var_map, n->id);
  if (!var) {
    var = gen_temp_var(&INT);
    hmap_put(v->var_map, n->id, var);
  }
  return var;
}

DEF_VISIT_FUNC(ast_ir, lopr_deref) {
  iropr_var_t *var = hmap_get(v->var_map, n->id);
  if (!var) {
    var = gen_temp_var(&INT);
    hmap_put(v->var_map, n->id, var);
  }
  return IROPRNEW(iropr_var, var->id, (type_t *)TYPENEW(type_ref, 4, &INT));
}

DEF_VISIT_FUNC(ast_ir, ropr_var) {
  iropr_var_t *var = hmap_get(v->var_map, n->id);
  if (!var) {
    var = gen_temp_var(&INT);
    hmap_put(v->var_map, n->id, var);
  }
  return var;
}

DEF_VISIT_FUNC(ast_ir, ropr_deref) {
  iropr_var_t *var = hmap_get(v->var_map, n->id), *tmp = gen_temp_var(&INT);
  if (!var) {
    var = gen_temp_var(&INT);
    hmap_put(v->var_map, n->id, var);
  }
  add_ir(IRNEW(ir_load, tmp, var));
  return tmp;
}

DEF_VISIT_FUNC(ast_ir, ropr_imm) {
  return IROPRNEW(iropr_imm, n->imm);
}

DEF_VISIT_FUNC(ast_ir, ropr_addr) {
  iropr_var_t *var = hmap_get(v->var_map, n->id);
  if (!var) {
    var = gen_temp_var(&INT);
    hmap_put(v->var_map, n->id, var);
  }
  return var;
}

#define AST_IR_FUNC(name, ...) ast_ir_##name,

static ast_visitor_table_t ast_ir_table = {
  ASTALL(AST_IR_FUNC)
};

void ast_ir() {
  init_ir_program();
  ast_ir_t ast_irer = {ast_ir_table, new_hmap(strsame, NULL, strhash, 0), new_hmap(strsame, NULL, strhash, 0), NULL, NULL};
  ast_visit(&ast_irer, ast_get_root());
}

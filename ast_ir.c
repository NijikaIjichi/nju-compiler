#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ir.h"
#include "ast.h"
#include "ast_visitor.h"

iropr_imm_t IMM0 = {E_iropr_imm, 0}, IMM1 = {E_iropr_imm, 1};

static iropr_t *iropr2atom(iropr_t *opr) {
  if (opr->oprid == E_iropr_var) {
    iropr_var_t *var = (void*)opr;
    if (var->type->typeid == E_type_ref) {
      type_ref_t *ref_type = (type_ref_t*)(var->type);
      if (ref_type->ref->typeid == E_type_int) {
        iropr_var_t *atom = gen_temp_var(ref_type->ref);
        add_ir(IRNEW(ir_load, atom, var));
        return (iropr_t *)atom;
      }
    }
  }
  return opr;
}

typedef struct ast_ir {
  void **table;
  int branch;
  ir_label_t *true_label, *false_label;
} ast_ir_t;

#define VTYPE ast_ir_t

#define ERR() ({printf("Cannot translate\n"); exit(0);})

//#define BAN31
//#define BAN32

#ifdef BAN31
#define ERR31() ERR()
#else
#define ERR31() ((void)0)
#endif

#ifdef BAN32
#define ERR32() ERR()
#else
#define ERR32() ((void)0)
#endif

DEF_VISIT_FUNC(ast_ir, program) {
  ast_visit(v, n->ext_def_list);
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, ext_def_list) {
  ast_visit(v, n->ext_def);
  ast_visit(v, n->ext_def_list);
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, ext_def__var) {
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, ext_def__none) {
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, ext_def__fun) {
  ast_visit(v, n->fun_dec);
  ast_visit(v, n->comp_st);
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, ext_dec_list) {
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, specifier__type) {
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, specifier__struct) {
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, struct_specifier__def) {
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, struct_specifier__tag) {
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, opt_tag) {
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, tag) {
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, var_dec__id) {
  var_t *var = n->scratch;
  if (var->id < 0) {
    var->id = new_var_id();
  }
  return var;
}

DEF_VISIT_FUNC(ast_ir, var_dec__array) {
  return ast_visit(v, n->var_dec);
}

DEF_VISIT_FUNC(ast_ir, fun_dec) {
  iropr_vars_t *param = ast_visit(v, n->var_list);
  add_cfg(IRNEW(ir_func, n->id, param));
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, var_list) {
  iropr_var_t *opr = ast_visit(v, n->param_dec);
  iropr_vars_t *param = ast_visit(v, n->var_list);
  return NEW(iropr_vars, opr, param);
}

DEF_VISIT_FUNC(ast_ir, param_dec) {
  var_t *var = ast_visit(v, n->var_dec);
  iropr_var_t *opr = IROPRNEW(iropr_var, var->id, var->type);
  switch (var->type->typeid) {
  case E_type_array: ERR32();
  case E_type_struct: opr->type = (type_t*)TYPENEW(type_ref, 4, opr->type);
  default:;
  }
  return opr;
}

DEF_VISIT_FUNC(ast_ir, comp_st) {
  ast_visit(v, n->def_list);
  ast_visit(v, n->stmt_list);
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, stmt_list) {
  ast_visit(v, n->stmt);
  ast_visit(v, n->stmt_list);
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, stmt__exp) {
  ast_visit(v, n->exp);
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, stmt__comp) {
  ast_visit(v, n->comp_st);
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, stmt__ret) {
  iropr_t *opr = iropr2atom(ast_visit(v, n->exp));
  add_ir(IRNEW(ir_ret, opr));
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, stmt__if) {
  ir_label_t *true_label = gen_label(), *false_label = gen_label();
  VTYPE v1 = {v->table, 1, true_label, false_label};
  ast_visit(&v1, n->exp);
  add_ir(true_label);
  ast_visit(v, n->stmt);
  add_ir(false_label);
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, stmt__ifelse) {
  ir_label_t *true_label = gen_label(), *false_label = gen_label(), *end = gen_label();
  VTYPE v1 = {v->table, 1, true_label, false_label};
  ast_visit(&v1, n->exp);
  add_ir(true_label);
  ast_visit(v, n->ifst);
  add_ir(IRNEW(ir_goto, end));
  add_ir(false_label);
  ast_visit(v, n->elsest);
  add_ir(end);
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, stmt__while) {
  ir_label_t *stmt = gen_label(), *end = gen_label();
  VTYPE v1 = {v->table, 1, stmt, end};
  ast_visit(&v1, n->exp);
  add_ir(stmt);
  ast_visit(v, n->stmt);
  ast_visit(&v1, n->exp);
  add_ir(end);
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, def_list) {
  ast_visit(v, n->def);
  ast_visit(v, n->def_list);
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, def) {
  ast_visit(v, n->dec_list);
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, dec_list) {
  ast_visit(v, n->dec);
  ast_visit(v, n->dec_list);
  return NULL;
}

void copy_ref(iropr_var_t *lhs, iropr_var_t *rhs) {
  assert(lhs->type->typeid == E_type_ref);
  type_ref_t *lhs_ref = (type_ref_t *)(lhs->type);
  assert(rhs->type->typeid == E_type_ref);
  type_ref_t *rhs_ref = (type_ref_t *)(rhs->type);
  int size = MIN(lhs_ref->ref->size, rhs_ref->ref->size);
  for (int i = 0; i < size; i += 4) {
    iropr_var_t *mrhs = gen_temp_var(&INT),
                *tmp = gen_temp_var(&INT),
                *mlhs = gen_temp_var(&INT);
    add_ir(IRNEW(ir_arth, mrhs, (void*)rhs, 
                  (void*)IROPRNEW(iropr_imm, i), OP2_PLUS));
    add_ir(IRNEW(ir_load, tmp, mrhs));
    add_ir(IRNEW(ir_arth, mlhs, (void*)lhs, 
                  (void*)IROPRNEW(iropr_imm, i), OP2_PLUS));
    add_ir(IRNEW(ir_store, mlhs, (void*)tmp));
  }
}

DEF_VISIT_FUNC(ast_ir, dec) {
  var_t *var = ast_visit(v, n->var_dec);
  iropr_var_t *lhs = IROPRNEW(iropr_var, var->id, var->type);
  switch (var->type->typeid) {
  case E_type_array: 
    if (((type_array_t*)(var->type))->next->typeid == E_type_array) ERR32();
  case E_type_struct: {
    iropr_var_t *mlhs = gen_temp_var(var->type);
    lhs->type = (type_t*)TYPENEW(type_ref, 4, lhs->type);
    add_ir(IRNEW(ir_alloc, mlhs, var->type->size));
    add_ir(IRNEW(ir_addr, lhs, mlhs));
    if (n->exp) {
      iropr_t *rhs = iropr2atom(ast_visit(v, n->exp));
      assert(rhs->oprid == E_iropr_var);
      copy_ref(lhs, (iropr_var_t *)rhs);
      break;
    }
  }
  default: if (n->exp) {
    iropr_t *rhs = iropr2atom(ast_visit(v, n->exp));
    add_ir(IRNEW(ir_mov, lhs, rhs));
  }
  }
  return NULL;
}

static void *ast_ir_exp_v2b(ast_ir_t *v, void *n) {
  assert(v->branch == 0);
  ir_label_t *true_label = gen_label(), *false_label = gen_label(), *end = gen_label();
  iropr_var_t *var = gen_temp_var(&INT);
  VTYPE v1 = {v->table, 1, true_label, false_label};
  ast_visit(&v1, n);
  add_ir(true_label);
  add_ir(IRNEW(ir_mov, var, (void*)&IMM1));
  add_ir(IRNEW(ir_goto, end));
  add_ir(false_label);
  add_ir(IRNEW(ir_mov, var, (void*)&IMM0));
  add_ir(end);
  return var;
}

static void *ast_ir_exp_b2v(ast_ir_t *v, void *n) {
  assert(v->branch == 1);
  VTYPE v1 = {v->table, 0, NULL, NULL};
  iropr_t *opr = iropr2atom(ast_visit(&v1, n));
  add_ir(IRNEW(ir_branch, opr, (void*)&IMM0, NEQ, v->true_label));
  add_ir(IRNEW(ir_goto, v->false_label));
  return NULL;
}

static void *ast_ir_exp__mov_v(ast_ir_t *v, exp__2op_t *n) {
  assert(v->branch == 0);
  iropr_var_t *lhs = ast_visit(v, n->lexp);
  iropr_t *rhs = iropr2atom(ast_visit(v, n->rexp));
  if (lhs->type->typeid == E_type_ref) {
    type_ref_t *lhs_ref = (type_ref_t *)(lhs->type);
    if (lhs_ref->ref->typeid == E_type_int) {
      add_ir(IRNEW(ir_store, lhs, rhs));
    } else {
      assert(rhs->oprid == E_iropr_var);
      copy_ref(lhs, (iropr_var_t *)rhs);
    }
  } else {
    assert(lhs->type->typeid == E_type_int);
    add_ir(IRNEW(ir_mov, lhs, rhs));
  }
  return lhs;
}

static void *ast_ir_exp__2oparth_v(ast_ir_t *v, exp__2op_t *n) {
  assert(v->branch == 0);
  iropr_t *lhs = iropr2atom(ast_visit(v, n->lexp));
  iropr_t *rhs = iropr2atom(ast_visit(v, n->rexp));
  iropr_var_t *res = gen_temp_var(&INT);
  add_ir(IRNEW(ir_arth, res, lhs, rhs, n->op));
  return res;
}

static void *ast_ir_exp__and_b(ast_ir_t *v, exp__2op_t *n) {
  assert(v->branch == 1);
  ir_label_t *label = gen_label();
  VTYPE v1 = {v->table, 1, label, v->false_label};
  ast_visit(&v1, n->lexp);
  add_ir(label);
  ast_visit(v, n->rexp);
  return NULL;
}

static void *ast_ir_exp__or_b(ast_ir_t *v, exp__2op_t *n) {
  assert(v->branch == 1);
  ir_label_t *label = gen_label();
  VTYPE v1 = {v->table, 1, v->true_label, label};
  ast_visit(&v1, n->lexp);
  add_ir(label);
  ast_visit(v, n->rexp);
  return NULL;
}

static void *ast_ir_exp__relop_b(ast_ir_t *v, exp__2op_t *n) {
  assert(v->branch == 1);
  VTYPE v1 = {v->table, 0, NULL, NULL};
  iropr_t *lhs = iropr2atom(ast_visit(&v1, n->lexp));
  iropr_t *rhs = iropr2atom(ast_visit(&v1, n->rexp));
  add_ir(IRNEW(ir_branch, lhs, rhs, n->relop, v->true_label));
  add_ir(IRNEW(ir_goto, v->false_label));
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, exp__2op) {
  if (v->branch == 0) {
    switch (n->op) {
    case OP2_ASSIGNOP: return ast_ir_exp__mov_v(v, n);
    case OP2_PLUS:
    case OP2_MINUS:
    case OP2_STAR:
    case OP2_DIV: return ast_ir_exp__2oparth_v(v, n);
    default: return ast_ir_exp_v2b(v, n);
    }
  } else {
    switch (n->op) {
    case OP2_AND: return ast_ir_exp__and_b(v, n);
    case OP2_OR: return ast_ir_exp__or_b(v, n);
    case OP2_RELOP: return ast_ir_exp__relop_b(v, n);
    default: return ast_ir_exp_b2v(v, n);
    }
  }
}

DEF_VISIT_FUNC(ast_ir, exp__para) {
  return ast_visit(v, n->exp);
}

static void *ast_ir_exp__minus_v(ast_ir_t *v, exp__1op_t *n) {
  assert(v->branch == 0);
  iropr_t *exp = iropr2atom(ast_visit(v, n->exp));
  iropr_var_t *res = gen_temp_var(&INT);
  add_ir(IRNEW(ir_arth, res, (void*)&IMM0, exp, OP2_MINUS));
  return res;
}

static void *ast_ir_exp__not_b(ast_ir_t *v, exp__1op_t *n) {
  assert(v->branch == 1);
  VTYPE v1 = {v->table, 0, NULL, NULL};
  iropr_t *opr = iropr2atom(ast_visit(&v1, n->exp));
  add_ir(IRNEW(ir_branch, opr, (void*)&IMM0, EQ, v->true_label));
  add_ir(IRNEW(ir_goto, v->false_label));
  return NULL;
}

DEF_VISIT_FUNC(ast_ir, exp__1op) {
  if (v->branch == 0) {
    switch (n->op) {
    case OP1_MINUS: return ast_ir_exp__minus_v(v, n);
    default: return ast_ir_exp_v2b(v, n);
    }
  } else {
    switch (n->op) {
    case OP1_NOT: return ast_ir_exp__not_b(v, n);
    default: return ast_ir_exp_b2v(v, n);
    }
  }
}

DEF_VISIT_FUNC(ast_ir, exp__call) {
  if (v->branch == 1) {
    return ast_ir_exp_b2v(v, n);
  } else {
    iroprs_t *args = ast_visit(v, n->args);
    if (strcmp(n->id, "read") == 0) {
      iropr_var_t *res = gen_temp_var(&INT);
      add_ir(IRNEW(ir_read, res));
      return res;
    } else if (strcmp(n->id, "write") == 0) {
      assert(args);
      add_ir(IRNEW(ir_write, args->opr));
      return &IMM0;
    } else {
      iropr_var_t *res = gen_temp_var(&INT);
      add_ir(IRNEW(ir_call, res, n->id, args));
      return res;
    }
  }
}

DEF_VISIT_FUNC(ast_ir, exp__array) {
  if (v->branch == 1) {
    return ast_ir_exp_b2v(v, n);
  }
  iropr_var_t *arr = ast_visit(v, n->arrexp);
  iropr_t *idx = iropr2atom(ast_visit(v, n->idxexp));
  assert(arr->type->typeid == E_type_ref);
  type_ref_t *reftype = (void*)(arr->type);
  assert(reftype->ref->typeid == E_type_array);
  type_array_t *arrtype = (void*)(reftype->ref);
  iropr_var_t *off = gen_temp_var(&INT);
  iropr_var_t *res = gen_temp_var((type_t*)TYPENEW(type_ref, 4, arrtype->next));
  iropr_imm_t *size = IROPRNEW(iropr_imm, arrtype->next->size);
  add_ir(IRNEW(ir_arth, off, idx, (void*)size, OP2_STAR));
  add_ir(IRNEW(ir_arth, res, (void*)arr, (void*)off, OP2_PLUS));
  return res;
}

DEF_VISIT_FUNC(ast_ir, exp__dot) {
  if (v->branch == 1) {
    return ast_ir_exp_b2v(v, n);
  }
  iropr_var_t *opr = ast_visit(v, n->exp);
  assert(opr->type->typeid == E_type_ref);
  type_ref_t *reftype = (void*)(opr->type);
  assert(reftype->ref->typeid == E_type_struct);
  type_struct_t *struct_type = (void*)(reftype->ref);
  var_t *field = lookup_var_deep(struct_type->struct_->fields, n->id);
  assert(field->offset >= 0);
  iropr_imm_t *off = IROPRNEW(iropr_imm, field->offset);
  iropr_var_t *res = gen_temp_var((type_t*)TYPENEW(type_ref, 4, field->type));
  add_ir(IRNEW(ir_arth, res, (void*)opr, (void*)off, OP2_PLUS));
  return res;
}

DEF_VISIT_FUNC(ast_ir, exp__id) {
  if (v->branch == 1) {
    return ast_ir_exp_b2v(v, n);
  }
  var_t *var = n->scratch;
  assert(var->id >= 0);
  switch (var->type->typeid) {
  case E_type_array:
  case E_type_struct:
    return IROPRNEW(iropr_var, var->id, (type_t*)TYPENEW(type_ref, 4, var->type));
  case E_type_int:
    return IROPRNEW(iropr_var, var->id, var->type);
  default:
    assert(0);
  }
}

DEF_VISIT_FUNC(ast_ir, exp__int) {
  if (v->branch == 1) {
    return ast_ir_exp_b2v(v, n);
  }
  return IROPRNEW(iropr_imm, n->ival);
}

DEF_VISIT_FUNC(ast_ir, exp__float) {
  assert(0);
}

DEF_VISIT_FUNC(ast_ir, args) {
  iroprs_t *args = ast_visit(v, n->args);
  iropr_t *opr = iropr2atom(ast_visit(v, n->exp));
  return NEW(iroprs, opr, args);
}

#define AST_IR_FUNC(name, ...) ast_ir_##name,

static ast_visitor_table_t ast_ir_table = {
  ASTALL(AST_IR_FUNC)
};

void ast_ir() {
  if (get_struct_list() != NULL) ERR31();
  ast_ir_t ast_semer = {ast_ir_table, 0, NULL, NULL};
  init_ir_program();
  ast_visit(&ast_semer, ast_get_root());
}

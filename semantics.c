#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "ast.h"
#include "ast_visitor.h"
#include "semantics.h"

type_t INT = {E_type_int, 4}, FLOAT = {E_type_float, 4}, ERROR = {E_type_error, 0};

frame_t global_frame = {NULL, NULL, 0};

void add_var(frame_t *frame, var_t *var) {
  frame->var_list = NEW(vars, var, frame->var_list);
  var->offset = frame->size;
  frame->size += var->type->size;
}

var_t *lookup_var(frame_t *frame, const char *name) {
  for (vars_t *l = frame->var_list; l; l = l->next) {
    if (strcmp(name, l->var->name) == 0) {
      return l->var;
    }
  }
  return NULL;
}

var_t *lookup_var_deep(frame_t *frame, const char *name) {
  for (frame_t *f = frame; f; f = f->parent) {
    var_t *v = lookup_var(f, name);
    if (v) return v;
  }
  return NULL;
}

int is_same_type(type_t *t1, type_t *t2) {
  if (t1->typeid == E_type_error || t2->typeid == E_type_error) return 2;
  if (t1->typeid != t2->typeid) return 0;
  switch (t1->typeid) {
  case E_type_int:
  case E_type_float: return 1;
  case E_type_array:
    return is_same_type(((type_array_t*)t1)->next, ((type_array_t*)t2)->next);
  case E_type_struct:
    return ((type_struct_t*)t1)->struct_ == ((type_struct_t*)t2)->struct_;
  case E_type_func: return 0;
  default: assert(0);
  }
}

void add_array(var_t *var, type_array_t *type) {
  type_t **t = &(var->type);
  while ((*t)->typeid == E_type_array) {
    (*t)->size *= type->num;
    t = &(((type_array_t*)(*t))->next);
  }
  type->next = *t;
  type->size = (*t)->size * type->num;
  *t = (type_t*)type;
}

typedef struct exp_type {
  type_t *type;
  int is_lv;
} exp_type_t;

structs_t *global_structs;

structs_t *get_struct_list() {
  return global_structs;
}

void add_struct(struct__t *struct_) {
  global_structs = NEW(structs, struct_, global_structs);
}

struct__t *lookup_struct(const char *name) {
  for (structs_t *p = global_structs; p; p = p->next) {
    if (p->struct_->name && strcmp(name, p->struct_->name) == 0) {
      return p->struct_;
    }
  }
  return NULL;
}

int is_name_dup(frame_t *frame, const char *name) {
  return lookup_struct(name) || lookup_var(frame, name);
}

int is_name_dup_deep(frame_t *frame, const char *name) {
  return lookup_struct(name) || lookup_var_deep(frame, name);
}

/*

typedef struct ast_semantics {
  void **table;
  type_t *type;
  frame_t *frame;
  int in_struct;
} ast_semantics_t;

#define VTYPE ast_semantics_t

void set_error();

#define ERR(type, line) \
  ({set_error(); printf("Error type %d at Line %d: semantics error\n", type, line);})

DEF_VISIT_FUNC(ast_semantics, program) {
  ast_visit(v, n->ext_def_list);
  return NULL;
}

DEF_VISIT_FUNC(ast_semantics, ext_def_list) {
  ast_visit(v, n->ext_def);
  ast_visit(v, n->ext_def_list);
  return NULL;
}

DEF_VISIT_FUNC(ast_semantics, ext_def__var) {
  VTYPE v1 = *v;
  v1.type = ast_visit(v, n->specifier);
  ast_visit(&v1, n->ext_dec_list);
  return NULL;
}

DEF_VISIT_FUNC(ast_semantics, ext_def__none) {
  ast_visit(v, n->specifier);
  return NULL;
}

DEF_VISIT_FUNC(ast_semantics, ext_def__fun) {
  VTYPE v1 = *v;
  v1.type = ast_visit(v, n->specifier);
  v1.frame = NEW(frame, NULL, v->frame, 0);
  ast_visit(&v1, n->fun_dec);
  ast_visit(&v1, n->comp_st);
  return NULL;
}

DEF_VISIT_FUNC(ast_semantics, ext_dec_list) {
  ast_visit(v, n->var_dec);
  ast_visit(v, n->ext_dec_list);
  return NULL;
}

DEF_VISIT_FUNC(ast_semantics, specifier__type) {
  switch (n->type) {
  case E_type_int: return &INT;
  case E_type_float: return &FLOAT;
  default: assert(0);
  }
}

DEF_VISIT_FUNC(ast_semantics, specifier__struct) {
  return ast_visit(v, n->struct_specifier);
}

DEF_VISIT_FUNC(ast_semantics, struct_specifier__def) {
  char *name = ast_visit(v, n->opt_tag);
  VTYPE v1 = *v;
  v1.frame = NEW(frame, NULL, NULL, 0);
  v1.in_struct = 1;
  ast_visit(&v1, n->def_list);
  struct__t *s = NEW(struct_, name, v1.frame);
  if (name && is_name_dup_deep(v->frame, name)) {
    ERR(16, n->lineno);
  }
  add_struct(s);
  return TYPENEW(type_struct, s->fields->size, s);
}

DEF_VISIT_FUNC(ast_semantics, struct_specifier__tag) {
  const char *name = ast_visit(v, n->tag);
  struct__t *s = lookup_struct(name);
  return s ? (void*)TYPENEW(type_struct, s->fields->size, s) : (void*)&ERROR;
}

DEF_VISIT_FUNC(ast_semantics, opt_tag) {
  return n->id;
}

DEF_VISIT_FUNC(ast_semantics, tag) {
  return n->id;
}

DEF_VISIT_FUNC(ast_semantics, var_dec__id) {
  if (is_name_dup(v->frame, n->id)) {
    if (v->in_struct) ERR(15, n->lineno);
    else ERR(3, n->lineno);
  }
  if (v->type->typeid == E_type_error) ERR(17, n->lineno);
  var_t *var = NEW(var, v->type, n->id, -1, -1);
  n->scratch = var;
  add_var(v->frame, var);
  return var;
}

DEF_VISIT_FUNC(ast_semantics, var_dec__array) {
  var_t *var = ast_visit(v, n->var_dec);
  assert(v->frame->var_list->var == var);
  v->frame->size -= var->type->size;
  add_array(var, TYPENEW(type_array, 0, n->ival, NULL));
  v->frame->size += var->type->size;
  return var;
}

DEF_VISIT_FUNC(ast_semantics, fun_dec) {
  if (is_name_dup(&global_frame, n->id)) {
    ERR(4, n->lineno);
  }
  vars_t *vars = ast_visit(v, n->var_list);
  type_func_t *tf = TYPENEW(type_func, 0, vars, v->type);
  var_t *vf = NEW(var, (void*)tf, n->id, -1, -1);
  add_var(&global_frame, vf);
  return NULL;
}

DEF_VISIT_FUNC(ast_semantics, var_list) {
  var_t *var = ast_visit(v, n->param_dec);
  vars_t *vars = ast_visit(v, n->var_list);
  return NEW(vars, var, vars);
}

DEF_VISIT_FUNC(ast_semantics, param_dec) {
  VTYPE v1 = *v;
  v1.type = ast_visit(v, n->specifier);
  return ast_visit(&v1, n->var_dec);
}

DEF_VISIT_FUNC(ast_semantics, comp_st) {
  ast_visit(v, n->def_list);
  ast_visit(v, n->stmt_list);
  return NULL;
}

DEF_VISIT_FUNC(ast_semantics, stmt_list) {
  ast_visit(v, n->stmt);
  ast_visit(v, n->stmt_list);
  return NULL;
}

DEF_VISIT_FUNC(ast_semantics, stmt__exp) {
  ast_visit(v, n->exp);
  return NULL;
}

DEF_VISIT_FUNC(ast_semantics, stmt__comp) {
  VTYPE v1 = *v;
  v1.frame = NEW(frame, NULL, v->frame, 0);
  ast_visit(&v1, n->comp_st);
  return NULL;
}

DEF_VISIT_FUNC(ast_semantics, stmt__ret) {
  exp_type_t *et = ast_visit(v, n->exp);
  if (!is_same_type(et->type, v->type)) {
    ERR(8, n->lineno);
  }
  return NULL;
}

DEF_VISIT_FUNC(ast_semantics, stmt__if) {
  exp_type_t *et = ast_visit(v, n->exp);
  if (!is_same_type(et->type, &INT)) {
    ERR(7, n->lineno);
  }
  ast_visit(v, n->stmt);
  return NULL;
}

DEF_VISIT_FUNC(ast_semantics, stmt__ifelse) {
  exp_type_t *et = ast_visit(v, n->exp);
  if (!is_same_type(et->type, &INT)) {
    ERR(7, n->lineno);
  }
  ast_visit(v, n->ifst);
  ast_visit(v, n->elsest);
  return NULL;
}

DEF_VISIT_FUNC(ast_semantics, stmt__while) {
  exp_type_t *et = ast_visit(v, n->exp);
  if (!is_same_type(et->type, &INT)) {
    ERR(7, n->lineno);
  }
  ast_visit(v, n->stmt);
  return NULL;
}

DEF_VISIT_FUNC(ast_semantics, def_list) {
  ast_visit(v, n->def);
  ast_visit(v, n->def_list);
  return NULL;
}

DEF_VISIT_FUNC(ast_semantics, def) {
  VTYPE v1 = *v;
  v1.type = ast_visit(v, n->specifier);
  ast_visit(&v1, n->dec_list);
  return NULL;
}

DEF_VISIT_FUNC(ast_semantics, dec_list) {
  ast_visit(v, n->dec);
  ast_visit(v, n->dec_list);
  return NULL;
}

DEF_VISIT_FUNC(ast_semantics, dec) {
  var_t *var = ast_visit(v, n->var_dec);
  if (n->exp) {
    if (v->in_struct) {
      ERR(15, n->lineno);
    } else {
      exp_type_t *et = ast_visit(v, n->exp);
      if (!is_same_type(var->type, et->type)) {
        ERR(5, n->lineno);
      }
    }
  }
  return NULL;
}

DEF_VISIT_FUNC(ast_semantics, exp__2op) {
  exp_type_t *et1 = ast_visit(v, n->lexp);
  exp_type_t *et2 = ast_visit(v, n->rexp);
  if (n->op == OP2_ASSIGNOP) {
    if (!et1->is_lv) {
      ERR(6, n->lineno);
    }
    if (!is_same_type(et1->type, et2->type)) {
      ERR(5, n->lineno);
    }
    return NEW(exp_type, et1->type, 1);
  } else {
    switch (is_same_type(et1->type, et2->type)) {
    case 1: switch (et1->type->typeid) {
            case E_type_float: if (n->op == OP2_AND || n->op == OP2_OR) goto E1;
            case E_type_int: 
              return NEW(exp_type, n->op == OP2_RELOP ? &INT : et1->type, 0);
            default: ;
            }
    case 0: E1: ERR(7, n->lineno);
    case 2: return NEW(exp_type, &ERROR, 0);
    default: assert(0);
    }
  }
}

DEF_VISIT_FUNC(ast_semantics, exp__para) {
  return ast_visit(v, n->exp);
}

DEF_VISIT_FUNC(ast_semantics, exp__1op) {
  exp_type_t *et = ast_visit(v, n->exp);
  switch (et->type->typeid) {
  case E_type_float: if (n->op == OP1_NOT) goto E1;
  case E_type_int: return NEW(exp_type, et->type, 0);
  default: E1: ERR(7, n->lineno);
  case E_type_error: return NEW(exp_type, &ERROR, 0);
  }
}

DEF_VISIT_FUNC(ast_semantics, exp__call) {
  var_t *var = lookup_var_deep(v->frame, n->id);
  if (!var) {
    ERR(2, n->lineno);
    return NEW(exp_type, &ERROR, 0);
  }
  switch (var->type->typeid) {
  case E_type_func: break;
  default: ERR(11, n->lineno);
  case E_type_error: return NEW(exp_type, &ERROR, 0);
  }
  type_func_t *func = (type_func_t*)(var->type);
  vars_t *params = func->arg_list;
  vars_t *args = ast_visit(v, n->args);
  while (1) {
    if (!params && !args) return NEW(exp_type, func->ret, 0);
    if (!params || !args) {
      ERR(9, n->lineno);
      return NEW(exp_type, func->ret, 0);
    }
    if (!is_same_type(params->var->type, args->var->type)) {
      ERR(9, n->lineno);
      return NEW(exp_type, func->ret, 0);
    }
    params = params->next;
    args = args->next;
  }
}

DEF_VISIT_FUNC(ast_semantics, exp__array) {
  exp_type_t *et1 = ast_visit(v, n->arrexp);
  exp_type_t *et2 = ast_visit(v, n->idxexp);
  switch (et1->type->typeid) {
  case E_type_array:
    if (!is_same_type(et2->type, &INT)) {
      ERR(12, n->lineno);
    }
    return NEW(exp_type, ((type_array_t*)(et1->type))->next, 1);
  default: ERR(10, n->lineno);
  case E_type_error: return NEW(exp_type, &ERROR, 1);
  }
}

DEF_VISIT_FUNC(ast_semantics, exp__dot) {
  exp_type_t *et = ast_visit(v, n->exp);
  switch (et->type->typeid) {
  case E_type_struct: {
    struct__t *s = ((type_struct_t*)(et->type))->struct_;
    var_t *var = lookup_var(s->fields, n->id);
    if (!var) {
      ERR(14, n->lineno);
      return NEW(exp_type, &ERROR, 1);
    }
    return NEW(exp_type, var->type, 1);
  }
  default: ERR(13, n->lineno);
  case E_type_error: return NEW(exp_type, &ERROR, 1);
  }
}

DEF_VISIT_FUNC(ast_semantics, exp__id) {
  var_t *var = lookup_var_deep(v->frame, n->id);
  n->scratch = var;
  if (!var) {
    ERR(1, n->lineno);
    return NEW(exp_type, &ERROR, 1);
  }
  return NEW(exp_type, var->type, 1);
}

DEF_VISIT_FUNC(ast_semantics, exp__int) {
  return NEW(exp_type, &INT, 0);
}

DEF_VISIT_FUNC(ast_semantics, exp__float) {
  return NEW(exp_type, &FLOAT, 0);
}

DEF_VISIT_FUNC(ast_semantics, args) {
  exp_type_t *et = ast_visit(v, n->exp);
  vars_t *vars = ast_visit(v, n->args);
  return NEW(vars, NEW(var, et->type, NULL, -1, -1), vars);
}

#define AST_SEM_FUNC(name, ...) ast_semantics_##name,

static ast_visitor_table_t ast_semantics_table = {
  ASTALL(AST_SEM_FUNC)
};

void ast_sem() {
  add_var(&global_frame, NEW(var, (void*)TYPENEW(type_func, 0, NULL, &INT), strdup("read"), -1, -1));
  add_var(&global_frame, NEW(var, (void*)TYPENEW(type_func, 0, NEW(vars, NEW(var, &INT, strdup("x"), 0, -1) , NULL), &INT), strdup("write"), -1, -1));
  ast_semantics_t ast_semer = {ast_semantics_table, &ERROR, &global_frame, 0};
  ast_visit(&ast_semer, ast_get_root());
}

*/

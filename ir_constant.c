#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "ir_visitor.h"
#include "ir.h"

#define UNDEF    NULL
#define NAC      ((ir_cval_t)2)
#define ISCON(x) ((uint32_t)(uint64_t)(x) == 3)
#define I2CON(x) ((ir_cval_t)(((uint64_t)(x) << 32) | 3))
#define CON2I(x) ((int)((uint64_t)(x) >> 32))
#define ZERO     I2CON(0)

static int inited = 0, do_opt = 0;

static void ir_constant_reinit(ir_program_t *program) {
  assert(inited);
  LIST(ir_cfg_t*) *cfgs = program->cfgs;
  for (int i = 0; i < cfgs->size; ++i) {
    ir_cfg_t *cfg = cfgs->array[i];
    if (!cfg->reachable) continue;
    LIST(ir_bb_t*) *bbs = cfg->bbs;
    ir_constant_res_t *res = &(cfg->constant_res);
    assert(worklist_empty(res->worklist));
    for (int j = 0; j < bbs->size; ++j) {
      ir_bb_t *bb = bbs->array[j];
      if (!bb->reachable) continue;
      int st = bb->range.start, ed = bb->range.end - 1;
      hmap_removeall(res->res[st].in);
      for (int k = st; k <= ed; ++k) {
        hmap_removeall(res->res[k].out);
        if (k != ed) assert(res->res[k + 1].in == res->res[k].out);
      }
    }
  }
}

static void ir_constant_init(ir_program_t *program) {
  do_opt = 0;
  if (inited) {
    ir_constant_reinit(program);
    return;
  } else {
    inited = 1;
  }
  LIST(ir_cfg_t*) *cfgs = program->cfgs;
  for (int i = 0; i < cfgs->size; ++i) {
    ir_cfg_t *cfg = cfgs->array[i];
    if (!cfg->reachable) continue;
    LIST(ir_bb_t*) *bbs = cfg->bbs;
    ir_constant_res_t *res = &(cfg->constant_res);
    res->res = calloc(cfg->irs->size + 1, sizeof(ir_df_map_t));
    res->worklist = new_worklist(bbs->size);
    res->buf = new_hmap(same_iropr, NULL, hash_iropr, 0);
    for (int j = 0; j < bbs->size; ++j) {
      ir_bb_t *bb = bbs->array[j];
      if (!bb->reachable) continue;
      int st = bb->range.start, ed = bb->range.end - 1;
      res->res[st].in = new_hmap(same_iropr, NULL, hash_iropr, 0);
      for (int k = st; k <= ed; ++k) {
        res->res[k].out = new_hmap(same_iropr, NULL, hash_iropr, 0);
        if (k != ed) res->res[k + 1].in = res->res[k].out;
      }
    }
  }
}

static void ir_constant_copy(ir_constant_res_t *res, int index) {
  hmap_copy(res->res[index].out, res->res[index].in);
}

static ir_cval_t map_get_constant(HMAP(iropr_var_t *, ir_cval_t) *map, iropr_t *opr) {
  if (opr->oprid == E_iropr_imm) {
    return I2CON(((iropr_imm_t *)opr)->val);
  } else {
    assert(opr->oprid == E_iropr_var);
    return hmap_get(map, opr);
  }
}

static ir_cval_t get_constant(ir_constant_res_t *res, int index, iropr_t *opr) {
  HMAP(iropr_var_t *, ir_cval_t) *map = res->res[index].in;
  return map_get_constant(map, opr);
}

static void set_constant(ir_constant_res_t *res, int index, 
    iropr_var_t *opr, ir_cval_t val) {
  HMAP(iropr_var_t *, ir_cval_t) *map = res->res[index].out;
  hmap_put(map, opr, val);
}

static iropr_t *to_constant(HMAP(iropr_var_t *, ir_cval_t) *map, iropr_t *opr) {
  if (opr->oprid == E_iropr_imm) {
    return opr;
  } else {
    assert(opr->oprid == E_iropr_var);
    ir_cval_t v = hmap_get(map, opr);
    if (ISCON(v)) {
      do_opt = 1;
      return (iropr_t *)IROPRNEW(iropr_imm, CON2I(v));
    } else if (v == UNDEF) {
      do_opt = 1;
      return (iropr_t *)IROPRNEW(iropr_imm, 0);
    } else {
      return opr;
    }
  }
}

static ir_cval_t calc_constant(ir_cval_t v1, ir_cval_t v2, op2_t op, relop_t relop) {
  if (op == OP2_STAR && (v1 == ZERO || v2 == ZERO)) {
    return ZERO;
  } else if (op == OP2_DIV && v2 == ZERO) {
    return UNDEF;
  }
  if (ISCON(v1) && ISCON(v2)) {
    int x = CON2I(v1), y = CON2I(v2);
    switch (op) {
    case OP2_PLUS: return I2CON(x + y);
    case OP2_MINUS: return I2CON(x - y);
    case OP2_STAR: return I2CON(x * y);
    case OP2_DIV: return I2CON(x / y);
    case OP2_RELOP: 
      switch (relop) {
      case GT: return I2CON(x > y);
      case LE: return I2CON(x <= y);
      case GE: return I2CON(x >= y);
      case LT: return I2CON(x < y);
      case EQ: return I2CON(x == y);
      case NEQ: return I2CON(x != y);
      default: assert(0);
      }
    default: assert(0);
    }
  } else if (v1 == UNDEF || v2 == UNDEF) {
    return UNDEF;
  } else {
    return NAC;
  }
}

static ir_cval_t consmap_comb(iropr_var_t *k, ir_cval_t v1, ir_cval_t v2) {
  if (v1 == NAC || v2 == NAC) {
    return NAC;
  } else if (v1 == UNDEF) {
    return v2;
  } else if (v2 == UNDEF) {
    return v1;
  } else if (v1 == v2) {
    return v1;
  } else {
    return NAC;
  }
}

static void ir_constant_meet(ir_constant_res_t *res, ir_bb_t *dst, ir_bb_t *src) {
  hmap_combine(BB_IN(res, dst), BB_OUT(res, src), consmap_comb);
}

typedef struct ir_constant {
  void **table;
  ir_constant_res_t *res;
  int index;
} ir_constant_t;

DEF_VISIT_FUNC(ir_constant, ir_nop) {
  return NULL;
}

DEF_VISIT_FUNC(ir_constant, ir_label) {
  assert(0);
}

DEF_VISIT_FUNC(ir_constant, ir_func) {
  for (iropr_vars_t *l = n->params; l; l = l->next) {
    set_constant(v->res, v->index, l->opr, NAC);
  }
  return NULL;
}

DEF_VISIT_FUNC(ir_constant, ir_mov) {
  set_constant(v->res, v->index, n->lhs, get_constant(v->res, v->index, n->rhs));
  return NULL;
}

DEF_VISIT_FUNC(ir_constant, ir_arth) {
  if (n->op == OP2_MINUS && same_iropr(n->opr1, n->opr2)) {
    set_constant(v->res, v->index, n->lhs, I2CON(0));
  } else if (n->op == OP2_DIV && same_iropr(n->opr1, n->opr2)) {
    set_constant(v->res, v->index, n->lhs, I2CON(1));
  } else {
    ir_cval_t v1 = get_constant(v->res, v->index, n->opr1), 
              v2 = get_constant(v->res, v->index, n->opr2);
    set_constant(v->res, v->index, n->lhs, calc_constant(v1, v2, n->op, 0));
  }
  return NULL;
}

DEF_VISIT_FUNC(ir_constant, ir_addr) {
  set_constant(v->res, v->index, n->lhs, NAC);
  return NULL;
}

DEF_VISIT_FUNC(ir_constant, ir_load) {
  set_constant(v->res, v->index, n->lhs, NAC);
  return NULL;
}

DEF_VISIT_FUNC(ir_constant, ir_store) {
  return NULL;
}

DEF_VISIT_FUNC(ir_constant, ir_goto) {
  return NULL;
}

DEF_VISIT_FUNC(ir_constant, ir_branch) {
  return NULL;
}

DEF_VISIT_FUNC(ir_constant, ir_ret) {
  return NULL;
}

DEF_VISIT_FUNC(ir_constant, ir_alloc) {
  return NULL;
}

DEF_VISIT_FUNC(ir_constant, ir_call) {
  set_constant(v->res, v->index, n->ret, NAC);
  return NULL;
}

DEF_VISIT_FUNC(ir_constant, ir_read) {
  set_constant(v->res, v->index, n->opr, NAC);
  return NULL;
}

DEF_VISIT_FUNC(ir_constant, ir_write) {
  return NULL;
}

#define IR_CONSTANT_FUNC(name, ...) ir_constant_##name,

static ir_visitor_table_t ir_constant_table = {
  IRALL(IR_CONSTANT_FUNC)
};

static int ir_constant_transfer_bb(ir_constant_res_t *res, 
    LIST(ir_t*) *irs, ir_bb_t *bb) {
  ir_constant_t visitor = {ir_constant_table, res, 0};
  int st = bb->range.start, ed = bb->range.end - 1;
  hmap_copy(res->buf, BB_OUT(res, bb));
  for (int i = st; i <= ed; ++i) {
    visitor.index = i;
    ir_constant_copy(res, i);
    ir_visit(&visitor, irs->array[i]);
  }
  return hmap_cmp(res->buf, BB_OUT(res, bb));
}

typedef struct ir_consfold {
  void **table;
  HMAP(iropr_var_t *, ir_cval_t) *in_map, *out_map;
  ir_t **ir_pos;
} ir_consfold_t;

DEF_VISIT_FUNC(ir_consfold, ir_nop) {
  return NULL;
}

DEF_VISIT_FUNC(ir_consfold, ir_label) {
  assert(0);
}

DEF_VISIT_FUNC(ir_consfold, ir_func) {
  return NULL;
}

DEF_VISIT_FUNC(ir_consfold, ir_mov) {
  n->rhs = to_constant(v->in_map, n->rhs);
  return NULL;
}

static ir_t *make_mov(iropr_var_t *lhs, iropr_t *rhs) {
  if (rhs->oprid == E_iropr_var && lhs->id == ((iropr_var_t *)rhs)->id) {
    return (ir_t *)IRNEW(ir_nop);
  } else {
    return (ir_t *)IRNEW(ir_mov, lhs, rhs);
  }
}

DEF_VISIT_FUNC(ir_consfold, ir_arth) {
  iropr_t *vlhs = to_constant(v->out_map, (iropr_t *)n->lhs);
  if (vlhs->oprid == E_iropr_imm) {
    v->ir_pos[0] = (ir_t *)IRNEW(ir_mov, n->lhs, vlhs);
  } else {
    n->opr1 = to_constant(v->in_map, n->opr1);
    n->opr2 = to_constant(v->in_map, n->opr2);
    switch (n->op) {
    case OP2_PLUS:
      if (is_iropr_imm(n->opr1, 0)) {
        do_opt = 1;
        v->ir_pos[0] = make_mov(n->lhs, n->opr2);
      } else if (is_iropr_imm(n->opr2, 0)) {
        do_opt = 1;
        v->ir_pos[0] = make_mov(n->lhs, n->opr1);
      } else if (n->opr1->oprid == E_iropr_imm) {
        iropr_t *opr = n->opr1;
        n->opr1 = n->opr2;
        n->opr2 = opr;
      }
      break;
    case OP2_MINUS:
      if (is_iropr_imm(n->opr2, 0)) {
        do_opt = 1;
        v->ir_pos[0] = make_mov(n->lhs, n->opr1);
      } else if (n->opr2->oprid == E_iropr_imm) {
        do_opt = 1;
        iropr_imm_t *imm = (iropr_imm_t *)(n->opr2);
        iropr_t *opr = (iropr_t *)IROPRNEW(iropr_imm, -imm->val);
        v->ir_pos[0] = (ir_t *)IRNEW(ir_arth, n->lhs, n->opr1, opr, OP2_PLUS);
      }
      break;
    case OP2_STAR:
      if (is_iropr_imm(n->opr1, 2)) {
        do_opt = 1;
        v->ir_pos[0] = (ir_t *)IRNEW(ir_arth, n->lhs, n->opr2, n->opr2, OP2_PLUS);
      } else if (is_iropr_imm(n->opr2, 2)) {
        do_opt = 1;
        v->ir_pos[0] = (ir_t *)IRNEW(ir_arth, n->lhs, n->opr1, n->opr1, OP2_PLUS);
      } else if (n->opr1->oprid == E_iropr_imm) {
        iropr_t *opr = n->opr1;
        n->opr1 = n->opr2;
        n->opr2 = opr;
      }
    case OP2_DIV:
      if (is_iropr_imm(n->opr2, 1)) {
        do_opt = 1;
        v->ir_pos[0] = make_mov(n->lhs, n->opr1);
      }
      break;
    default: assert(0);
    }
  }
  return NULL;
}

DEF_VISIT_FUNC(ir_consfold, ir_addr) {
  return NULL;
}

DEF_VISIT_FUNC(ir_consfold, ir_load) {
  return NULL;
}

DEF_VISIT_FUNC(ir_consfold, ir_store) {
  n->rhs = to_constant(v->in_map, n->rhs);
  return NULL;
}

DEF_VISIT_FUNC(ir_consfold, ir_goto) {
  return NULL;
}

DEF_VISIT_FUNC(ir_consfold, ir_branch) {
  ir_cval_t v1 = map_get_constant(v->in_map, n->opr1),
            v2 = map_get_constant(v->in_map, n->opr2);
  ir_cval_t val = calc_constant(v1, v2, OP2_RELOP, n->op);
  if (val == UNDEF || (ISCON(val) && CON2I(val) == 0)) {
    do_opt = 1;
    remove_branch_goto((ir_t *)n);
  } else if (ISCON(val)) {
    do_opt = 1;
    ir_goto_t *newir = IRNEW(ir_goto, n->label);
    add_branch_goto((ir_t *)newir);
    v->ir_pos[0] = (ir_t *)newir;
    remove_branch_goto((ir_t *)n);
  } else {
    n->opr1 = to_constant(v->in_map, n->opr1);
    n->opr2 = to_constant(v->in_map, n->opr2);
    if (n->opr1->oprid == E_iropr_imm) {
      iropr_t *opr = n->opr1;
      n->opr1 = n->opr2;
      n->opr2 = opr;
      if (n->op <= 3) {
        n->op = 3 - n->op;
      }
    }
  }
  return NULL;
}

DEF_VISIT_FUNC(ir_consfold, ir_ret) {
  n->opr = to_constant(v->in_map, n->opr);
  return NULL;
}

DEF_VISIT_FUNC(ir_consfold, ir_alloc) {
  return NULL;
}

DEF_VISIT_FUNC(ir_consfold, ir_call) {
  for (iroprs_t *l = n->args; l; l = l->next) {
    l->opr = to_constant(v->in_map, l->opr);
  }
  return NULL;
}

DEF_VISIT_FUNC(ir_consfold, ir_read) {
  return NULL;
}

DEF_VISIT_FUNC(ir_consfold, ir_write) {
  n->opr = to_constant(v->in_map, n->opr);
  return NULL;
}

#define IR_CONFLOD_FUNC(name, ...) ir_consfold_##name,

static ir_visitor_table_t ir_consfold_table = {
  IRALL(IR_CONFLOD_FUNC)
};

static void ir_consfold_bb(ir_cfg_t *cfg, ir_bb_t *bb) {
  ir_consfold_t visitor = {ir_consfold_table, NULL, NULL, NULL};
  ir_constant_res_t *res = &cfg->constant_res;
  LIST(ir_t*) *irs = cfg->irs;
  int st = bb->range.start, ed = bb->range.end - 1;
  for (int i = st; i <= ed; ++i) {
    visitor.in_map = res->res[i].in;
    visitor.out_map = res->res[i].out;
    visitor.ir_pos = (ir_t **)&(irs->array[i]);
    ir_visit(&visitor, irs->array[i]);
  }
}

int ir_constant() {
  ir_program_t *program = get_ir_program();
  ir_constant_init(program);
  LIST(ir_cfg_t*) *cfgs = program->cfgs;
  for (int i = 0; i < cfgs->size; ++i) {
    ir_cfg_t *cfg = cfgs->array[i];
    if (!cfg->reachable) continue;
    ir_iter_cfg(cfg, &cfg->constant_res, cfg->constant_res.worklist,
      ir_constant_meet, NULL, ir_constant_transfer_bb, 1);
    ir_analyse_cfg(cfg, ir_consfold_bb);
    build_cfg(cfg);
  }
  check_program_reachable();
  return do_opt;
}

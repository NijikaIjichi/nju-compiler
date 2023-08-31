#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "ir_visitor.h"
#include "ir.h"

static int inited = 0, do_opt = 0;

static void ir_livevar_reinit(ir_program_t *program) {
  assert(inited);
  LIST(ir_cfg_t*) *cfgs = program->cfgs;
  for (int i = 0; i < cfgs->size; ++i) {
    ir_cfg_t *cfg = cfgs->array[i];
    if (!cfg->reachable) continue;
    LIST(ir_bb_t*) *bbs = cfg->bbs;
    ir_livevar_res_t *res = &cfg->livevar_res;
    bitset_zero(res->cross_call);
    assert(worklist_empty(res->worklist));
    for (int j = 0; j < bbs->size; ++j) {
      ir_bb_t *bb = bbs->array[j];
      if (!bb->reachable) continue;
      int st = bb->range.start, ed = bb->range.end - 1;
      bitset_zero(res->res[st].in);
      for (int k = st; k <= ed; ++k) {
        bitset_zero(res->res[k].out);
        if (k != ed) assert(res->res[k + 1].in == res->res[k].out);
      }
    }
    bitset_zero(BB_IN(res, cfg->exit));
  }
}

static void ir_livevar_init(ir_program_t *program) {
  do_opt = 0;
  if (inited) {
    ir_livevar_reinit(program);
    return;
  } else {
    inited = 1;
  }
  LIST(ir_cfg_t*) *cfgs = program->cfgs;
  int vars = program->var_num;
  for (int i = 0; i < cfgs->size; ++i) {
    ir_cfg_t *cfg = cfgs->array[i];
    if (!cfg->reachable) continue;
    LIST(ir_bb_t*) *bbs = cfg->bbs;
    ir_livevar_res_t *res = &cfg->livevar_res;
    res->res = calloc(cfg->irs->size + 1, sizeof(ir_df_bs_t));
    res->cross_call = new_bitset(vars, 0);
    res->worklist = new_worklist(bbs->size);
    res->buf = new_bitset(vars, 0);
    for (int j = 0; j < bbs->size; ++j) {
      ir_bb_t *bb = bbs->array[j];
      if (!bb->reachable) continue;
      int st = bb->range.start, ed = bb->range.end - 1;
      res->res[st].in = new_bitset(vars, 0);
      for (int k = st; k <= ed; ++k) {
        res->res[k].out = new_bitset(vars, 0);
        if (k != ed) res->res[k + 1].in = res->res[k].out;
      }
    }
    BB_IN(res, cfg->exit) = new_bitset(vars, 0);
  }
}

static void ir_livevar_copy(ir_livevar_res_t *res, int index) {
  bitset_copy(res->res[index].in, res->res[index].out);
}

static void ir_livevar_gen(ir_livevar_res_t *res, int index, iropr_var_t *gen) {
  bitset_set(res->res[index].in, gen->id);
}

static int ir_livevar_kill(ir_livevar_res_t *res, int index, iropr_var_t *kill) {
  int r = bitset_test(res->res[index].in, kill->id);
  bitset_clear(res->res[index].in, kill->id);
  return r;
}

static void ir_livevar_meet(ir_livevar_res_t *res, ir_bb_t *dst, ir_bb_t *src) {
  bitset_or(BB_OUT(res, dst), BB_IN(res, src));
}

typedef struct ir_livevar {
  void **table;
  ir_livevar_res_t *res;
  int index;
} ir_livevar_t;

DEF_VISIT_FUNC(ir_livevar, ir_nop) {
  return NULL;
}

DEF_VISIT_FUNC(ir_livevar, ir_label) {
  assert(0);
}

DEF_VISIT_FUNC(ir_livevar, ir_func) {
  for (iropr_vars_t *l = n->params; l; l = l->next) {
    ir_livevar_kill(v->res, v->index, l->opr);
  }
  return NULL;
}

DEF_VISIT_FUNC(ir_livevar, ir_mov) {
  if (ir_livevar_kill(v->res, v->index, n->lhs)) {
    if (n->rhs->oprid == E_iropr_var) {
      ir_livevar_gen(v->res, v->index, (iropr_var_t *)n->rhs);
    }
  }
  return NULL;
}

DEF_VISIT_FUNC(ir_livevar, ir_arth) {
  if (ir_livevar_kill(v->res, v->index, n->lhs)) {
    if (n->opr1->oprid == E_iropr_var) {
      ir_livevar_gen(v->res, v->index, (iropr_var_t *)n->opr1);
    }
    if (n->opr2->oprid == E_iropr_var) {
      ir_livevar_gen(v->res, v->index, (iropr_var_t *)n->opr2);
    }
  }
  return NULL;
}

DEF_VISIT_FUNC(ir_livevar, ir_addr) {
  if (ir_livevar_kill(v->res, v->index, n->lhs)) {
    ir_livevar_gen(v->res, v->index, n->rhs);
  }
  return NULL;
}

DEF_VISIT_FUNC(ir_livevar, ir_load) {
  if (ir_livevar_kill(v->res, v->index, n->lhs)) {
    ir_livevar_gen(v->res, v->index, n->rhs);
  }
  return NULL;
}

DEF_VISIT_FUNC(ir_livevar, ir_store) {
  ir_livevar_gen(v->res, v->index, n->lhs);
  if (n->rhs->oprid == E_iropr_var) {
    ir_livevar_gen(v->res, v->index, (iropr_var_t *)n->rhs);
  }
  return NULL;
}

DEF_VISIT_FUNC(ir_livevar, ir_goto) {
  return NULL;
}

DEF_VISIT_FUNC(ir_livevar, ir_branch) {
  if (n->opr1->oprid == E_iropr_var) {
    ir_livevar_gen(v->res, v->index, (iropr_var_t *)n->opr1);
  }
  if (n->opr2->oprid == E_iropr_var) {
    ir_livevar_gen(v->res, v->index, (iropr_var_t *)n->opr2);
  }
  return NULL;
}

DEF_VISIT_FUNC(ir_livevar, ir_ret) {
  if (n->opr->oprid == E_iropr_var) {
    ir_livevar_gen(v->res, v->index, (iropr_var_t *)n->opr);
  }
  return NULL;
}

DEF_VISIT_FUNC(ir_livevar, ir_alloc) {
  ir_livevar_kill(v->res, v->index, n->opr);
  return NULL;
}

DEF_VISIT_FUNC(ir_livevar, ir_call) {
  ir_livevar_kill(v->res, v->index, n->ret);
  for (iroprs_t *l = n->args; l; l = l->next) {
    if (l->opr->oprid == E_iropr_var) {
      ir_livevar_gen(v->res, v->index, (iropr_var_t *)(l->opr));
    }
  }
  return NULL;
}

DEF_VISIT_FUNC(ir_livevar, ir_read) {
  ir_livevar_kill(v->res, v->index, n->opr);
  return NULL;
}

DEF_VISIT_FUNC(ir_livevar, ir_write) {
  if (n->opr->oprid == E_iropr_var) {
    ir_livevar_gen(v->res, v->index, (iropr_var_t *)n->opr);
  }
  return NULL;
}

#define IR_LIVEVAR_FUNC(name, ...) ir_livevar_##name,

static ir_visitor_table_t ir_livevar_table = {
  IRALL(IR_LIVEVAR_FUNC)
};

static int ir_livevar_transfer_bb(ir_livevar_res_t *res, 
    LIST(ir_t*) *irs, ir_bb_t *bb) {
  ir_livevar_t visitor = {ir_livevar_table, res, 0};
  int ed = bb->range.end - 1, st = bb->range.start;
  bitset_copy(res->buf, BB_IN(res, bb));
  for (int i = ed; i >= st; --i) {
    visitor.index = i;
    ir_livevar_copy(res, i);
    ir_visit(&visitor, irs->array[i]);
  }
  return bitset_cmp(res->buf, BB_IN(res, bb));
}

static void ir_livevar_cross_call_bb(ir_cfg_t *cfg, ir_bb_t *bb) {
  ir_livevar_res_t *res = &(cfg->livevar_res); 
  LIST(ir_t*) *irs = cfg->irs;
  int ed = bb->range.end - 1, st = bb->range.start;
  for (int i = st; i <= ed; ++i) {
    ir_t *ir = irs->array[i];
    switch (ir->irid) {
    case E_ir_call:
    case E_ir_read:
    case E_ir_write:
      bitset_copy(res->buf, res->res[i].in);
      bitset_and(res->buf, res->res[i].out);
      bitset_or(res->cross_call, res->buf);
    default: ;
    }
  }
}

static void ir_livevar_elim_bb(ir_cfg_t *cfg, ir_bb_t *bb) {
  ir_livevar_res_t *res = &(cfg->livevar_res); 
  LIST(ir_t*) *irs = cfg->irs;
  int ed = bb->range.end - 1, st = bb->range.start;
  for (int i = st; i <= ed; ++i) {
    ir_t *ir = irs->array[i];
    iropr_var_t **lhs = NULL;
    switch (ir->irid) {
    case E_ir_mov: lhs = &((ir_mov_t *)ir)->lhs; break;
    case E_ir_arth: lhs = &((ir_arth_t *)ir)->lhs; break;
    case E_ir_addr: lhs = &((ir_addr_t *)ir)->lhs; break;
    case E_ir_load: lhs = &((ir_load_t *)ir)->lhs; break;
    case E_ir_alloc: lhs = &((ir_alloc_t *)ir)->opr; break;
    default: continue;
    }
    if (!bitset_test(res->res[i].out, (*lhs)->id)) {
      do_opt = 1;
      ir->irid = E_ir_nop;
    }
  }
}

static void ir_livevar_elim2_bb(ir_cfg_t *cfg, ir_bb_t *bb) {
  ir_livevar_res_t *res = &(cfg->livevar_res); 
  LIST(ir_t*) *irs = cfg->irs;
  int ed = bb->range.end - 1, st = bb->range.start;
  for (int i = ed - 1; i >= st; --i) {
    int j = i + 1;
    while (j <= ed && ((ir_t *)(irs->array[j]))->irid == E_ir_nop) j++;
    if (j > ed) return;
    ir_t *ir = irs->array[i], *next = irs->array[j];
    if (next->irid != E_ir_mov) continue;
    ir_mov_t *mov = (ir_mov_t *)next;
    if (mov->rhs->oprid == E_iropr_imm) continue;
    iropr_var_t **lhs = NULL, *rhs = (iropr_var_t *)(mov->rhs);
    switch (ir->irid) {
    case E_ir_mov: lhs = &((ir_mov_t *)ir)->lhs; break;
    case E_ir_arth: lhs = &((ir_arth_t *)ir)->lhs; break;
    case E_ir_addr: lhs = &((ir_addr_t *)ir)->lhs; break;
    case E_ir_load: lhs = &((ir_load_t *)ir)->lhs; break;
    case E_ir_call: lhs = &((ir_call_t *)ir)->ret; break;
    case E_ir_read: lhs = &((ir_read_t *)ir)->opr; break;
    default: continue;
    }
    if ((*lhs)->id == rhs->id && 
        !bitset_test(res->res[j].out, (*lhs)->id)) {
      do_opt = 1;
      *lhs = mov->lhs;
      mov->irid = E_ir_nop;
    }
  }
}

int ir_livevar(int final) {
  ir_program_t *program = get_ir_program();
  ir_livevar_init(program);
  LIST(ir_cfg_t*) *cfgs = program->cfgs;
  for (int i = 0; i < cfgs->size; ++i) {
    ir_cfg_t *cfg = cfgs->array[i];
    if (!cfg->reachable) continue;
    ir_iter_cfg(cfg, &cfg->livevar_res, cfg->livevar_res.worklist,
      ir_livevar_meet, NULL, ir_livevar_transfer_bb, 0);
    ir_analyse_cfg(cfg, ir_livevar_elim_bb);
    if (final) ir_analyse_cfg(cfg, ir_livevar_elim2_bb);
    if (final && !do_opt) ir_analyse_cfg(cfg, ir_livevar_cross_call_bb);
  }
  return do_opt;
}

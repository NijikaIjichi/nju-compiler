#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "ir_visitor.h"
#include "ir.h"

static int inited = 0, do_opt = 0;

static void ir_avexpr_reinit(ir_program_t *program) {
  assert(inited);
  LIST(ir_cfg_t*) *cfgs = program->cfgs;
  for (int i = 0; i < cfgs->size; ++i) {
    ir_cfg_t *cfg = cfgs->array[i];
    if (!cfg->reachable) continue;
    LIST(ir_bb_t*) *bbs = cfg->bbs;
    ir_avexpr_res_t *res = &(cfg->avexpr_res);
    assert(worklist_empty(res->worklist));
    for (int j = 0; j < bbs->size; ++j) {
      ir_bb_t *bb = bbs->array[j];
      if (!bb->reachable) continue;
      int st = bb->range.start, ed = bb->range.end - 1;
      hmap_removeall(res->res[st].in);
      res->res[st].in->is_top = 1;
      for (int k = st; k <= ed; ++k) {
        hmap_removeall(res->res[k].out);
        res->res[k].out->is_top = 1;
        if (k != ed) assert(res->res[k + 1].in == res->res[k].out);
      }
    }
    assert(bbs->size > 0);
    ir_bb_t *startbb = bbs->array[0];
    BB_IN(res, startbb)->is_top = 0;
  }
}

static void ir_avexpr_init(ir_program_t *program) {
  do_opt = 0;
  if (inited) {
    ir_avexpr_reinit(program);
    return;
  } else {
    inited = 1;
  }
  LIST(ir_cfg_t*) *cfgs = program->cfgs;
  for (int i = 0; i < cfgs->size; ++i) {
    ir_cfg_t *cfg = cfgs->array[i];
    if (!cfg->reachable) continue;
    LIST(ir_bb_t*) *bbs = cfg->bbs;
    ir_avexpr_res_t *res = &(cfg->avexpr_res);
    res->res = calloc(cfg->irs->size + 1, sizeof(ir_df_map_t));
    res->worklist = new_worklist(bbs->size);
    res->buf = new_hmap(same_ir_arth, same_iropr, hash_ir_arth, 0);
    for (int j = 0; j < bbs->size; ++j) {
      ir_bb_t *bb = bbs->array[j];
      if (!bb->reachable) continue;
      int st = bb->range.start, ed = bb->range.end - 1;
      res->res[st].in = new_hmap(same_ir_arth, same_iropr, hash_ir_arth, 1);
      for (int k = st; k <= ed; ++k) {
        res->res[k].out = new_hmap(same_ir_arth, same_iropr, hash_ir_arth, 1);
        if (k != ed) res->res[k + 1].in = res->res[k].out;
      }
    }
    assert(bbs->size > 0);
    ir_bb_t *startbb = bbs->array[0];
    BB_IN(res, startbb)->is_top = 0;
  }
}

static void ir_avexpr_copy(ir_avexpr_res_t *res, int index) {
  hmap_copy(res->res[index].out, res->res[index].in);
}

static void ir_avexpr_gen_mov(ir_avexpr_res_t *res, int index, ir_mov_t *gen) {
  if (!same_iropr((iropr_t *)gen->lhs, gen->rhs)) {
    hmap_put(res->res[index].out, IRNEW(ir_arth, gen->lhs, gen->rhs, 
      (iropr_t *)&IMM0, OP2_PLUS), gen->lhs);
  }
  ir_arth_t arth = {E_ir_arth, 0, gen->rhs, (iropr_t *)&IMM0, OP2_PLUS};
  iropr_var_t *nrhs;
  while ((nrhs = hmap_get(res->res[index].in, &arth))) {
    arth.opr1 = (iropr_t *)nrhs;
    if (!same_iropr((iropr_t *)gen->lhs, (iropr_t *)nrhs)) {
      hmap_put(res->res[index].out, IRNEW(ir_arth, gen->lhs, (iropr_t *)nrhs, 
        (iropr_t *)&IMM0, OP2_PLUS), gen->lhs);
    }
  }
}

static void ir_avexpr_arth_opr2(ir_avexpr_res_t *res, int index, ir_arth_t *gen) {
  if (!same_iropr((iropr_t *)gen->lhs, gen->opr2)) {
    hmap_put(res->res[index].out, gen, gen->lhs);
  }
  ir_arth_t arth = {E_ir_arth, 0, gen->opr2, (iropr_t *)&IMM0, OP2_PLUS};
  iropr_var_t *nrhs;
  while ((nrhs = hmap_get(res->res[index].in, &arth))) {
    arth.opr1 = (iropr_t *)nrhs;
    if (!same_iropr((iropr_t *)gen->lhs, (iropr_t *)nrhs)) {
      hmap_put(res->res[index].out, IRNEW(ir_arth, gen->lhs, gen->opr1, 
        (iropr_t *)nrhs, gen->op), gen->lhs);
    }
  }
}

static void ir_avexpr_gen_arth(ir_avexpr_res_t *res, int index, ir_arth_t *gen) {
  if (!same_iropr((iropr_t *)gen->lhs, gen->opr1)) {
    hmap_put(res->res[index].out, gen, gen->lhs);
  }
  ir_arth_t arth = {E_ir_arth, 0, gen->opr1, (iropr_t *)&IMM0, OP2_PLUS};
  iropr_var_t *nrhs;
  while ((nrhs = hmap_get(res->res[index].in, &arth))) {
    arth.opr1 = (iropr_t *)nrhs;
    if (!same_iropr((iropr_t *)gen->lhs, (iropr_t *)nrhs)) {
      ir_avexpr_arth_opr2(res, index, IRNEW(ir_arth, gen->lhs, (iropr_t *)nrhs, 
        gen->opr2, gen->op));
    }
  }
}

static iropr_var_t *kill_var;

static int kill_condfunc(ir_arth_t *k, iropr_var_t *v) {
  return same_iropr((iropr_t *)kill_var, k->opr1) 
    || same_iropr((iropr_t *)kill_var, k->opr2)
    || kill_var->id == v->id;
}

static void ir_avexpr_kill(ir_avexpr_res_t *res, int index, iropr_var_t *kill) {
  kill_var = kill;
  hmap_removeif(res->res[index].out, kill_condfunc);
}

static void ir_avexpr_meet(ir_avexpr_res_t *res, ir_bb_t *dst, ir_bb_t *src) {
  hmap_and(BB_IN(res, dst), BB_OUT(res, src));
}

typedef struct ir_avexpr {
  void **table;
  ir_avexpr_res_t *res;
  int index;
} ir_avexpr_t;

DEF_VISIT_FUNC(ir_avexpr, ir_nop) {
  return NULL;
}

DEF_VISIT_FUNC(ir_avexpr, ir_label) {
  assert(0);
}

DEF_VISIT_FUNC(ir_avexpr, ir_func) {
  return NULL;
}

DEF_VISIT_FUNC(ir_avexpr, ir_mov) {
  ir_avexpr_kill(v->res, v->index, n->lhs);
  ir_avexpr_gen_mov(v->res, v->index, n);
  return NULL;
}

DEF_VISIT_FUNC(ir_avexpr, ir_arth) {
  ir_avexpr_kill(v->res, v->index, n->lhs);
  ir_avexpr_gen_arth(v->res, v->index, n);
  return NULL;
}

DEF_VISIT_FUNC(ir_avexpr, ir_addr) {
  ir_avexpr_kill(v->res, v->index, n->lhs);
  return NULL;
}

DEF_VISIT_FUNC(ir_avexpr, ir_load) {
  ir_avexpr_kill(v->res, v->index, n->lhs);
  return NULL;
}

DEF_VISIT_FUNC(ir_avexpr, ir_store) {
  return NULL;
}

DEF_VISIT_FUNC(ir_avexpr, ir_goto) {
  return NULL;
}

DEF_VISIT_FUNC(ir_avexpr, ir_branch) {
  return NULL;
}

DEF_VISIT_FUNC(ir_avexpr, ir_ret) {
  return NULL;
}

DEF_VISIT_FUNC(ir_avexpr, ir_alloc) {
  return NULL;
}

DEF_VISIT_FUNC(ir_avexpr, ir_call) {
  ir_avexpr_kill(v->res, v->index, n->ret);
  return NULL;
}

DEF_VISIT_FUNC(ir_avexpr, ir_read) {
  ir_avexpr_kill(v->res, v->index, n->opr);
  return NULL;
}

DEF_VISIT_FUNC(ir_avexpr, ir_write) {
  return NULL;
}

#define IR_AVEXPR_FUNC(name, ...) ir_avexpr_##name,

static ir_visitor_table_t ir_avexpr_table = {
  IRALL(IR_AVEXPR_FUNC)
};

static int ir_avexpr_transfer_bb(ir_avexpr_res_t *res, 
    LIST(ir_t*) *irs, ir_bb_t *bb) {
  ir_avexpr_t visitor = {ir_avexpr_table, res, 0};
  int st = bb->range.start, ed = bb->range.end - 1;
  hmap_copy(res->buf, BB_OUT(res, bb));
  for (int i = st; i <= ed; ++i) {
    visitor.index = i;
    ir_avexpr_copy(res, i);
    ir_visit(&visitor, irs->array[i]);
  }
  return hmap_cmp(res->buf, BB_OUT(res, bb));
}

static int ir_avexpr_skip(ir_avexpr_res_t *res, ir_bb_t *bb) {
  return BB_IN(res, bb)->is_top;
}

static iropr_var_t *ir_avexpr_arth_get2(ir_avexpr_res_t *res, int index, ir_arth_t *gen) {
  ir_arth_t target = {E_ir_arth, 0, gen->opr1, gen->opr2, gen->op};
  ir_arth_t arth = {E_ir_arth, 0, gen->opr2, (iropr_t *)&IMM0, OP2_PLUS};
  iropr_var_t *nrhs, *rv;
  while ((rv = hmap_get(res->res[index].in, &target)) == NULL) {
    if ((nrhs = hmap_get(res->res[index].in, &arth))) {
      arth.opr1 = (iropr_t *)nrhs;
      target.opr2 = (iropr_t *)nrhs;
    } else {
      return NULL;
    }
  }
  return rv;
}

static iropr_var_t *ir_avexpr_get_arth(ir_avexpr_res_t *res, int index, ir_arth_t *gen) {
  ir_arth_t target = {E_ir_arth, 0, gen->opr1, gen->opr2, gen->op};
  ir_arth_t arth = {E_ir_arth, 0, gen->opr1, (iropr_t *)&IMM0, OP2_PLUS};
  iropr_var_t *nrhs, *rv;
  while ((rv = ir_avexpr_arth_get2(res, index, &target)) == NULL) {
    if ((nrhs = hmap_get(res->res[index].in, &arth))) {
      arth.opr1 = (iropr_t *)nrhs;
      target.opr1 = (iropr_t *)nrhs;
    } else {
      return NULL;
    }
  }
  return rv;
}

static void ir_avexpr_elim_bb(ir_cfg_t *cfg, ir_bb_t *bb) {
  ir_avexpr_res_t *res = &cfg->avexpr_res;
  LIST(ir_t*) *irs = cfg->irs;
  int ed = bb->range.end - 1, st = bb->range.start;
  for (int i = st; i <= ed; ++i) {
    ir_t *ir = irs->array[i];
    if (ir->irid == E_ir_arth) {
      ir_arth_t *arth = (ir_arth_t *)ir;
      iropr_var_t *get = ir_avexpr_get_arth(res, i, arth);
      if (get) {
        do_opt = 1;
        if (same_iropr((iropr_t *)(arth->lhs), (iropr_t *)get)) {
          ir->irid = E_ir_nop;
        } else {
          irs->array[i] = IRNEW(ir_mov, arth->lhs, (iropr_t *)get);
        }
      }
    }
  }
}

static iropr_t *reverse_fold(HMAP(ir_arth_t *, iropr_var_t *) *map, iropr_t *opr) {
  if (opr->oprid == E_iropr_imm) {
    return opr;
  }
  iropr_var_t *var = (iropr_var_t *)opr, *nvar;
  ir_arth_t arth = {E_ir_arth, var, (iropr_t *)var, (iropr_t *)&IMM0, OP2_PLUS};
  while ((nvar = hmap_get(map, &arth))) {
    var = nvar;
    arth.opr1 = (iropr_t *)nvar;
  }
  if (!same_iropr((iropr_t *)var, opr)) {
    do_opt = 1;
  }
  return (iropr_t *)var;
}

typedef struct ir_revefold {
  void **table;
  HMAP(iropr_var_t *, iropr_var_t *) *in_map;
} ir_revefold_t;

DEF_VISIT_FUNC(ir_revefold, ir_nop) {
  return NULL;
}

DEF_VISIT_FUNC(ir_revefold, ir_label) {
  assert(0);
}

DEF_VISIT_FUNC(ir_revefold, ir_func) {
  return NULL;
}

DEF_VISIT_FUNC(ir_revefold, ir_mov) {
  n->rhs = reverse_fold(v->in_map, n->rhs);
  if (same_iropr((iropr_t *)(n->lhs), n->rhs)) {
    do_opt = 1;
    n->irid = E_ir_nop;
  }
  return NULL;
}

DEF_VISIT_FUNC(ir_revefold, ir_arth) {
  n->opr1 = reverse_fold(v->in_map, n->opr1);
  n->opr2 = reverse_fold(v->in_map, n->opr2);
  return NULL;
}

DEF_VISIT_FUNC(ir_revefold, ir_addr) {
  return NULL;
}

DEF_VISIT_FUNC(ir_revefold, ir_load) {
  n->rhs = (iropr_var_t *)reverse_fold(v->in_map, (iropr_t *)n->rhs);
  assert(n->rhs->oprid == E_iropr_var);
  return NULL;
}

DEF_VISIT_FUNC(ir_revefold, ir_store) {
  n->lhs = (iropr_var_t *)reverse_fold(v->in_map, (iropr_t *)n->lhs);
  assert(n->lhs->oprid == E_iropr_var);
  n->rhs = reverse_fold(v->in_map, n->rhs);
  return NULL;
}

DEF_VISIT_FUNC(ir_revefold, ir_goto) {
  return NULL;
}

DEF_VISIT_FUNC(ir_revefold, ir_branch) {
  n->opr1 = reverse_fold(v->in_map, n->opr1);
  n->opr2 = reverse_fold(v->in_map, n->opr2);
  return NULL;
}

DEF_VISIT_FUNC(ir_revefold, ir_ret) {
  n->opr = reverse_fold(v->in_map, n->opr);
  return NULL;
}

DEF_VISIT_FUNC(ir_revefold, ir_alloc) {
  return NULL;
}

DEF_VISIT_FUNC(ir_revefold, ir_call) {
  for (iroprs_t *l = n->args; l; l = l->next) {
    l->opr = reverse_fold(v->in_map, l->opr);
  }
  return NULL;
}

DEF_VISIT_FUNC(ir_revefold, ir_read) {
  return NULL;
}

DEF_VISIT_FUNC(ir_revefold, ir_write) {
  n->opr = reverse_fold(v->in_map, n->opr);
  return NULL;
}

#define IR_REVEFOLD_FUNC(name, ...) ir_revefold_##name,

static ir_visitor_table_t ir_revefold_table = {
  IRALL(IR_REVEFOLD_FUNC)
};

static void ir_revefold_bb(ir_cfg_t *cfg, ir_bb_t *bb) {
  ir_revefold_t visitor = {ir_revefold_table, NULL};
  ir_avexpr_res_t *res = &cfg->avexpr_res;
  LIST(ir_t*) *irs = cfg->irs;
  int st = bb->range.start, ed = bb->range.end - 1;
  for (int i = st; i <= ed; ++i) {
    visitor.in_map = res->res[i].in;
    ir_visit(&visitor, irs->array[i]);
  }
}

int ir_avexpr(int final) {
  ir_program_t *program = get_ir_program();
  ir_avexpr_init(program);
  LIST(ir_cfg_t*) *cfgs = program->cfgs;
  for (int i = 0; i < cfgs->size; ++i) {
    ir_cfg_t *cfg = cfgs->array[i];
    if (!cfg->reachable) continue;
    ir_iter_cfg(cfg, &cfg->avexpr_res, cfg->avexpr_res.worklist,
      ir_avexpr_meet, ir_avexpr_skip, ir_avexpr_transfer_bb, 1);
    ir_analyse_cfg(cfg, ir_avexpr_elim_bb);
    if (final) ir_analyse_cfg(cfg, ir_revefold_bb);
  }
  return do_opt;
}

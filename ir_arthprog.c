#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "ir_visitor.h"
#include "ir.h"

typedef struct vv {
  int var1, var2;
  op2_t op;
} vv_t;

typedef struct ivi {
  int var, mul, add;
} ivi_t;

typedef union val_union {
  struct {
    uint64_t type: 1;
    uint64_t var1: 15;
    uint64_t var2: 15;
    uint64_t op: 3;
  } vv;
  struct {
    uint64_t type: 1;
    uint64_t var: 15;
    uint64_t mul: 16;
    uint64_t add: 32;
  } ivi;
  ir_cval_t cv;
} val_union_t;

static ir_cval_t make_ivi(int var, int mul, int add) {
  if (var > 32768 || mul > 32767 || mul < -32768) return NULL;
  if (mul == 0) var = 0;
  return ((val_union_t){.ivi = {1, var, mul, add}}).cv;
}

static ir_cval_t make_vv(int var1, int var2, op2_t op) {
  assert(op == OP2_PLUS || op == OP2_MINUS);
  if (var1 > 32768 || var2 > 32768) return NULL;
  if (var1 == var2) {
    if (op == OP2_PLUS) {
      return make_ivi(var1, 2, 0);
    } else {
      return make_ivi(0, 0, 0);
    }
  }
  if (op == OP2_PLUS && var1 > var2) {
    int var3 = var1;
    var1 = var2;
    var2 = var3;
  }
  return ((val_union_t){.vv = {0, var1, var2, op}}).cv;
}

static int contain_var(ir_cval_t cv, int var) {
  val_union_t val = {.cv = cv};
  if (val.vv.type == 0) {
    return var == (int)val.vv.var1 || var == (int)val.vv.var2;
  } else {
    return var == (int)val.ivi.var;
  }
}

static int decode_ivi(ir_cval_t cv, int *var, int *mul, int *add) {
  val_union_t val = {.cv = cv};
  if (val.ivi.type != 1) return 0;
  if (var) *var = (int)(uint32_t)(val.ivi.var);
  if (mul) *mul = (int)(int16_t)(uint16_t)(val.ivi.mul);
  if (add) *add = (int)(val.ivi.add);
  return 1;
}

static int decode_vv(ir_cval_t cv, int *var1, int *var2, op2_t *op) {
  val_union_t val = {.cv = cv};
  if (val.vv.type != 0) return 0;
  if (var1) *var1 = (int)(uint32_t)(val.vv.var1);
  if (var2) *var2 = (int)(uint32_t)(val.vv.var2);
  if (op) *op = (op2_t)(uint32_t)(val.vv.op);
  return 1;
}

static int inited = 0, do_opt = 0, final = 0;

static void ir_arthprog_reinit(ir_program_t *program) {
  assert(inited);
  LIST(ir_cfg_t*) *cfgs = program->cfgs;
  for (int i = 0; i < cfgs->size; ++i) {
    ir_cfg_t *cfg = cfgs->array[i];
    if (!cfg->reachable) continue;
    LIST(ir_bb_t*) *bbs = cfg->bbs;
    ir_arthprog_res_t *res = &(cfg->arthprog_res);
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

static void ir_arthprog_init(ir_program_t *program, int fi) {
  do_opt = 0;
  final = fi;
  assert(sizeof(void *) == 8);
  assert(sizeof(val_union_t) == 8);
  if (inited) {
    ir_arthprog_reinit(program);
    return;
  } else {
    inited = 1;
  }
  LIST(ir_cfg_t*) *cfgs = program->cfgs;
  for (int i = 0; i < cfgs->size; ++i) {
    ir_cfg_t *cfg = cfgs->array[i];
    if (!cfg->reachable) continue;
    LIST(ir_bb_t*) *bbs = cfg->bbs;
    ir_arthprog_res_t *res = &(cfg->arthprog_res);
    res->res = calloc(cfg->irs->size + 1, sizeof(ir_df_map_t));
    res->worklist = new_worklist(bbs->size);
    res->buf = new_hmap(same_iropr, NULL, hash_iropr, 0);
    for (int j = 0; j < bbs->size; ++j) {
      ir_bb_t *bb = bbs->array[j];
      if (!bb->reachable) continue;
      int st = bb->range.start, ed = bb->range.end - 1;
      res->res[st].in = new_hmap(same_iropr, NULL, hash_iropr, 1);
      for (int k = st; k <= ed; ++k) {
        res->res[k].out = new_hmap(same_iropr, NULL, hash_iropr, 1);
        if (k != ed) res->res[k + 1].in = res->res[k].out;
      }
    }
    assert(bbs->size > 0);
    ir_bb_t *startbb = bbs->array[0];
    BB_IN(res, startbb)->is_top = 0;
  }
}

static void ir_arthprog_copy(ir_arthprog_res_t *res, int index) {
  hmap_copy(res->res[index].out, res->res[index].in);
}

static void ir_arthprog_gen_mov(ir_arthprog_res_t *res, int index, ir_mov_t *gen) {
  if (gen->rhs->oprid == E_iropr_var) {
    iropr_var_t *rhs = (iropr_var_t *)(gen->rhs);
    if (gen->lhs->id != rhs->id) {
      hmap_put(res->res[index].out, gen->lhs, make_ivi(rhs->id, 1, 0));
    }
  } else {
    iropr_imm_t *rhs = (iropr_imm_t *)(gen->rhs);
    hmap_put(res->res[index].out, gen->lhs, make_ivi(0, 0, rhs->val));
  }
}

static int is_addi(ir_arth_t *ir) {
  return ir->op == OP2_PLUS && ir->opr1->oprid == E_iropr_var 
    && ir->opr2->oprid == E_iropr_imm;
}

static int is_muli(ir_arth_t *ir) {
  return ir->op == OP2_STAR && ir->opr1->oprid == E_iropr_var 
    && ir->opr2->oprid == E_iropr_imm;
}

static int is_isub(ir_arth_t *ir) {
  return ir->op == OP2_MINUS && ir->opr1->oprid == E_iropr_imm 
    && ir->opr2->oprid == E_iropr_var;
}

static int is_addv(ir_arth_t *ir) {
  return ir->op == OP2_PLUS && ir->opr1->oprid == E_iropr_var 
    && ir->opr2->oprid == E_iropr_var;
}

static int is_subv(ir_arth_t *ir) {
  return ir->op == OP2_MINUS && ir->opr1->oprid == E_iropr_var 
    && ir->opr2->oprid == E_iropr_var;
}

static ir_cval_t arth2cval(ir_arth_t *gen) {
  if (is_addi(gen)) {
    iropr_var_t *opr1 = (iropr_var_t *)(gen->opr1);
    iropr_imm_t *opr2 = (iropr_imm_t *)(gen->opr2);
    if (gen->lhs->id != opr1->id) {
      return make_ivi(opr1->id, 1, opr2->val);
    } else {
      return NULL;
    }
  } else if (is_isub(gen)) {
    iropr_imm_t *opr1 = (iropr_imm_t *)(gen->opr1);
    iropr_var_t *opr2 = (iropr_var_t *)(gen->opr2);
    if (gen->lhs->id != opr2->id) {
      return make_ivi(opr2->id, -1, opr1->val);
    } else {
      return NULL;
    }
  } else if (is_muli(gen)) {
    iropr_var_t *opr1 = (iropr_var_t *)(gen->opr1);
    iropr_imm_t *opr2 = (iropr_imm_t *)(gen->opr2);
    if (gen->lhs->id != opr1->id) {
      return make_ivi(opr1->id, opr2->val, 0);
    } else {
      return NULL;
    }
  } else if (is_addv(gen) || is_subv(gen)) {
    iropr_var_t *opr1 = (iropr_var_t *)(gen->opr1);
    iropr_var_t *opr2 = (iropr_var_t *)(gen->opr2);
    if (gen->lhs->id != opr1->id && gen->lhs->id != opr2->id) {
      return make_vv(opr1->id, opr2->id, gen->op);
    } else {
      return NULL;
    }
  } else {
    return NULL;
  }
}

static void ir_arthprog_gen_arth(ir_arthprog_res_t *res, int index, ir_arth_t *gen) {
  hmap_put(res->res[index].out, gen->lhs, arth2cval(gen));
}

static int kill_id;

static int kill_condfunc(iropr_var_t *k, ir_cval_t v) {
  return contain_var(v, kill_id);
}

static void ir_arthprog_kill(ir_arthprog_res_t *res, int index, iropr_var_t *kill) {
  kill_id = kill->id;
  hmap_removeif(res->res[index].out, kill_condfunc);
}

static void ir_arthprog_kill_l(ir_arthprog_res_t *res, int index, iropr_var_t *kill) {
  hmap_remove(res->res[index].out, kill);
}

static void ir_arthprog_meet(ir_arthprog_res_t *res, ir_bb_t *dst, ir_bb_t *src) {
  hmap_and(BB_IN(res, dst), BB_OUT(res, src));
}

typedef struct ir_arthprog {
  void **table;
  ir_arthprog_res_t *res;
  int index;
} ir_arthprog_t;

DEF_VISIT_FUNC(ir_arthprog, ir_nop) {
  return NULL;
}

DEF_VISIT_FUNC(ir_arthprog, ir_label) {
  assert(0);
}

DEF_VISIT_FUNC(ir_arthprog, ir_func) {
  return NULL;
}

DEF_VISIT_FUNC(ir_arthprog, ir_mov) {
  ir_arthprog_gen_mov(v->res, v->index, n);
  ir_arthprog_kill(v->res, v->index, n->lhs);
  return NULL;
}

DEF_VISIT_FUNC(ir_arthprog, ir_arth) {
  ir_arthprog_gen_arth(v->res, v->index, n);
  ir_arthprog_kill(v->res, v->index, n->lhs);
  return NULL;
}

DEF_VISIT_FUNC(ir_arthprog, ir_addr) {
  ir_arthprog_kill_l(v->res, v->index, n->lhs);
  ir_arthprog_kill(v->res, v->index, n->lhs);
  return NULL;
}

DEF_VISIT_FUNC(ir_arthprog, ir_load) {
  ir_arthprog_kill_l(v->res, v->index, n->lhs);
  ir_arthprog_kill(v->res, v->index, n->lhs);
  return NULL;
}

DEF_VISIT_FUNC(ir_arthprog, ir_store) {
  return NULL;
}

DEF_VISIT_FUNC(ir_arthprog, ir_goto) {
  return NULL;
}

DEF_VISIT_FUNC(ir_arthprog, ir_branch) {
  return NULL;
}

DEF_VISIT_FUNC(ir_arthprog, ir_ret) {
  return NULL;
}

DEF_VISIT_FUNC(ir_arthprog, ir_alloc) {
  return NULL;
}

DEF_VISIT_FUNC(ir_arthprog, ir_call) {
  ir_arthprog_kill_l(v->res, v->index, n->ret);
  ir_arthprog_kill(v->res, v->index, n->ret);
  return NULL;
}

DEF_VISIT_FUNC(ir_arthprog, ir_read) {
  ir_arthprog_kill_l(v->res, v->index, n->opr);
  ir_arthprog_kill(v->res, v->index, n->opr);
  return NULL;
}

DEF_VISIT_FUNC(ir_arthprog, ir_write) {
  return NULL;
}

#define IR_arthprog_FUNC(name, ...) ir_arthprog_##name,

static ir_visitor_table_t ir_arthprog_table = {
  IRALL(IR_arthprog_FUNC)
};

static int ir_arthprog_transfer_bb(ir_arthprog_res_t *res, 
    LIST(ir_t*) *irs, ir_bb_t *bb) {
  ir_arthprog_t visitor = {ir_arthprog_table, res, 0};
  int st = bb->range.start, ed = bb->range.end - 1;
  hmap_copy(res->buf, BB_OUT(res, bb));
  for (int i = st; i <= ed; ++i) {
    visitor.index = i;
    ir_arthprog_copy(res, i);
    ir_visit(&visitor, irs->array[i]);
  }
  return hmap_cmp(res->buf, BB_OUT(res, bb));
}

static int ir_arthprog_skip(ir_arthprog_res_t *res, ir_bb_t *bb) {
  return BB_IN(res, bb)->is_top;
}

static ir_cval_t fold_v2v(HMAP(iropr_var_t *, ir_cval_t) *map, int from, int to) {
  int m = 1, imm = 0;
  iropr_var_t var = {E_iropr_var};
  while (1) {
    if (from == to) return make_ivi(from, m, imm);
    var.id = from;
    ir_cval_t r1 = hmap_get(map, &var);
    int v1, m1, imm1;
    if (decode_ivi(r1, &v1, &m1, &imm1)) {
      imm += imm1 * m;
      m *= m1;
      from = v1;
    } else return NULL;
  }
}

static ir_cval_t fold_to(HMAP(iropr_var_t *, ir_cval_t) *map, 
  ir_cval_t ori, int level, int lhsid) {
  if (!ori) return NULL;
  int v[2] = {0, 0}, m[2] = {1, 0}, imm = 0;
  op2_t op = 0;
  ir_cval_t res = ori;
  iropr_var_t var = {E_iropr_var};
  if (decode_vv(ori, &v[0], &v[1], &op)) {
    m[1] = op == OP2_PLUS ? 1 : -1;
  } else {
    assert(decode_ivi(ori, &v[0], &m[0], &imm) == 1);
  }
  while (1) {
    for (int i = 0; i < 2; ++i) {
      if (m[i] != 0) {
        var.id = v[i];
        ir_cval_t r1 = hmap_get(map, &var);
        if (!r1) continue;
        int tv[2], m0, imm0;
        op2_t op0;
        if (decode_vv(r1, &tv[0], &tv[1], &op0)) {
          ir_cval_t tmp;
          int v1 = 0, m1 = 0, imm1 = 0;
          if (m[1-i] != 0 && (tmp = fold_v2v(map, tv[0], v[1-i]))
            && decode_ivi(tmp, &v1, &m1, &imm1)
            && (m[1-i] + m[i] * m1 == 0
                || (imm != 0 && imm + m[i] * imm1 == 0))) {
            assert(v1 == v[1-i]);
            m[1-i] += m[i] * m1;
            imm += m[i] * imm1;
            v[i] = tv[1];
            m[i] = op0 == OP2_PLUS ? m[i] : -m[i];
            goto found;
          } else if (m[1-i] != 0 && (tmp = fold_v2v(map, tv[1], v[1-i]))
            && decode_ivi(tmp, &v1, &m1, &imm1)
            && (m[1-i] + (op0 == OP2_PLUS ? m[i] : -m[i]) * m1 == 0
                || (imm != 0 && imm + (op0 == OP2_PLUS ? m[i] : -m[i]) * imm1 == 0))) {
            assert(v1 == v[1-i]);
            m[1-i] += (op0 == OP2_PLUS ? m[i] : -m[i]) * m1;
            imm += (op0 == OP2_PLUS ? m[i] : -m[i]) * imm1;
            v[i] = tv[0];
            goto found;
          }
        } else {
          assert(decode_ivi(r1, &tv[0], &m0, &imm0) == 1);
          if (m[1-i] != 0 && tv[0] == v[1-i]) {
            m[1-i] += m0 * m[i];
            imm += imm0 * m[i];
            m[i] = 0;
            goto found;
          } else if (m0 == 0 || (imm != 0 && imm + imm0 * m[i] == 0)) {
            v[i] = tv[0];
            imm += imm0 * m[i];
            m[i] *= m0;
            goto found;
          }
        }
      }
    }
    for (int i = 0; i < 2; ++i) {
      if (m[i] != 0) {
        var.id = v[i];
        ir_cval_t r1 = hmap_get(map, &var);
        if (!r1) continue;
        int tv[2], m0, imm0;
        op2_t op0;
        if (decode_vv(r1, &tv[0], &tv[1], &op0)) {
          ir_cval_t tmp;
          int v1, m1, imm1;
          if (m[1-i] != 0 && (tmp = fold_v2v(map, tv[0], v[1-i]))
            && decode_ivi(tmp, &v1, &m1, &imm1)
            && (imm != 0 || imm1 == 0)) {
            assert(v1 == v[1-i]);
            m[1-i] += m[i] * m1;
            imm += m[i] * imm1;
            v[i] = tv[1];
            m[i] = op0 == OP2_PLUS ? m[i] : -m[i];
            goto found;
          } else if (m[1-i] != 0 && (tmp = fold_v2v(map, tv[1], v[1-i]))
            && decode_ivi(tmp, &v1, &m1, &imm1)
            && (imm != 0 || imm1 == 0)) {
            assert(v1 == v[1-i]);
            m[1-i] += (op0 == OP2_PLUS ? m[i] : -m[i]) * m1;
            imm += (op0 == OP2_PLUS ? m[i] : -m[i]) * imm1;
            v[i] = tv[0];
            goto found;
          }
        } else {
          assert(decode_ivi(r1, &tv[0], &m0, &imm0) == 1);
          if (m[1-i] != 0 && tv[0] == v[1-i]) {
            m[1-i] += m0 * m[i];
            imm += imm0 * m[i];
            m[i] = 0;
            goto found;
          } else if (imm != 0 || imm0 == 0) {
            v[i] = tv[0];
            imm += imm0 * m[i];
            m[i] *= m0;
            goto found;
          }
        }
      }
    }
    for (int i = 0; i < 2; ++i) {
      if (m[i] != 0) {
        var.id = v[i];
        ir_cval_t r1 = hmap_get(map, &var);
        if (!r1) continue;
        int tv[2], m0, imm0;
        op2_t op0;
        if (decode_vv(r1, &tv[0], &tv[1], &op0)) {
          ir_cval_t tmp;
          int v1, m1, imm1;
          if (m[1-i] == 0) {
            v[i] = tv[0];
            v[1-i] = tv[1];
            m[1-i] = op0 == OP2_PLUS ? m[i] : -m[i];
            goto found;
          } else if ((tmp = fold_v2v(map, tv[0], v[1-i]))) {
            assert(decode_ivi(tmp, &v1, &m1, &imm1));
            assert(v1 == v[1-i]);
            m[1-i] += m[i] * m1;
            imm += m[i] * imm1;
            v[i] = tv[1];
            m[i] = op0 == OP2_PLUS ? m[i] : -m[i];
            goto found;
          } else if ((tmp = fold_v2v(map, tv[1], v[1-i]))) {
            assert(decode_ivi(tmp, &v1, &m1, &imm1));
            assert(v1 == v[1-i]);
            m[1-i] += (op0 == OP2_PLUS ? m[i] : -m[i]) * m1;
            imm += (op0 == OP2_PLUS ? m[i] : -m[i]) * imm1;
            v[i] = tv[0];
            goto found;
          }
        } else {
          assert(decode_ivi(r1, &tv[0], &m0, &imm0) == 1);
          if (m[1-i] != 0 && tv[0] == v[1-i]) {
            m[1-i] += m0 * m[i];
            imm += imm0 * m[i];
            m[i] = 0;
            goto found;
          } else {
            v[i] = tv[0];
            imm += imm0 * m[i];
            m[i] *= m0;
            goto found;
          }
        }
      }
    }
    break;
found:
    if ((m[0] != 0 && v[0] == lhsid) || (m[1] != 0 && v[1] == lhsid)) continue;
    if (m[0] == 0 && m[1] == 0) {
      res = make_ivi(0, 0, imm);
    } else if (m[0] == 1 && m[1] == 0 && imm == 0) {
      res = make_ivi(v[0], 1, 0);
    } else if (m[0] == 0 && m[1] == 1 && imm == 0) {
      res = make_ivi(v[1], 1, 0);
    } else if (level > 0) {
      if (m[0] == 1 && m[1] == 1 && imm == 0) {
        res = make_vv(v[0], v[1], OP2_PLUS);
      } else if (m[0] == 1 && m[1] == -1 && imm == 0) {
        res = make_vv(v[0], v[1], OP2_MINUS);
      } else if (m[0] == -1 && m[1] == 1 && imm == 0) {
        res = make_vv(v[1], v[0], OP2_MINUS);
      } else if (abs(m[0]) == 1 && m[1] == 0) {
        res = make_ivi(v[0], m[0], imm);
      } else if (m[0] == 0 && abs(m[1]) == 1) {
        res = make_ivi(v[1], m[1], imm);
      } else if (m[1] == 0 && imm == 0 && m[0] >= -32768 && m[0] <= 32767) {
        res = make_ivi(v[0], m[0], 0);
      } else if (m[0] == 0 && imm == 0 && m[1] >= -32768 && m[1] <= 32767) {
        res = make_ivi(v[1], m[1], 0);
      }
    }
    assert(res);
  }
  return res;
}

static iropr_t *cval2opr(ir_cval_t cv) {
  int varid, mul, add;
  if (!cv || !decode_ivi(cv, &varid, &mul, &add)) return NULL;
  if (mul == 0) {
    return (iropr_t *)IROPRNEW(iropr_imm, add);
  } else if (mul == 1 && add == 0) {
    return (iropr_t *)IROPRNEW(iropr_var, varid);
  } else {
    return NULL;
  }
}

static iropr_t *try_fold(HMAP(iropr_var_t *, ir_cval_t) *map, 
  iropr_t *opr, int lhsid) {
  if (opr->oprid == E_iropr_imm) {
    return opr;
  } else {
    assert(opr->oprid == E_iropr_var);
    iropr_var_t *o = (iropr_var_t *)opr;
    iropr_t *res = cval2opr(fold_to(map, make_ivi(o->id, 1, 0), 0, lhsid));
    if (res && !same_iropr(opr, res)) {
      do_opt = 1;
      return res;
    }
    return opr;
  }
}

static ir_t *cval2ir(ir_cval_t cv, iropr_var_t *lhs) {
  iropr_t *opr = cval2opr(cv);
  if (opr) {
    if (same_iropr((iropr_t *)lhs, opr)) {
      return (ir_t *)IRNEW(ir_nop);
    } else {
      return (ir_t *)IRNEW(ir_mov, lhs, opr);
    }
  }
  int v1, v2, m1, imm;
  op2_t op;
  if (decode_vv(cv, &v1, &v2, &op)) {
    iropr_t *opr1 = (iropr_t *)IROPRNEW(iropr_var, v1);
    iropr_t *opr2 = (iropr_t *)IROPRNEW(iropr_var, v2);
    return (ir_t *)IRNEW(ir_arth, lhs, opr1, opr2, op);
  } else {
    assert(decode_ivi(cv, &v1, &m1, &imm));
    iropr_t *opr1 = (iropr_t *)IROPRNEW(iropr_var, v1);
    if (abs(m1) != 1) {
      assert(imm == 0 && m1 != 0);
      if (m1 == 2) {
        return (ir_t *)IRNEW(ir_arth, lhs, opr1, opr1, OP2_PLUS);
      } else {
        return (ir_t *)IRNEW(ir_arth, lhs, opr1, 
          (iropr_t *)IROPRNEW(iropr_imm, m1), OP2_STAR);
      }
    } else {
      iropr_t *immopr = (iropr_t *)IROPRNEW(iropr_imm, imm);
      if (m1 == 1) {
        return (ir_t *)IRNEW(ir_arth, lhs, opr1, immopr, OP2_PLUS);
      } else {
        return (ir_t *)IRNEW(ir_arth, lhs, immopr, opr1, OP2_MINUS);
      }
    }
  }
}

typedef struct ir_arthsimp {
  void **table;
  HMAP(iropr_var_t *, iropr_var_t *) *in_map, *out_map;
  ir_t **ir_pos;
} ir_arthsimp_t;

DEF_VISIT_FUNC(ir_arthsimp, ir_nop) {
  return NULL;
}

DEF_VISIT_FUNC(ir_arthsimp, ir_label) {
  assert(0);
}

DEF_VISIT_FUNC(ir_arthsimp, ir_func) {
  return NULL;
}

DEF_VISIT_FUNC(ir_arthsimp, ir_mov) {
  ir_cval_t cv;
  if (final && (cv = fold_to(v->in_map, hmap_get(v->out_map, n->lhs), 1, -1))) {
    ir_t *ir = cval2ir(cv, n->lhs);
    if (ir->irid != E_ir_mov || !same_iropr(((ir_mov_t *)ir)->rhs, n->rhs)) {
      do_opt = 1;
      v->ir_pos[0] = ir;
      return NULL;
    }
  }
  n->rhs = try_fold(v->in_map, n->rhs, -1);
  if (same_iropr((iropr_t *)(n->lhs), n->rhs)) {
    do_opt = 1;
    n->irid = E_ir_nop;
  }
  return NULL;
}

DEF_VISIT_FUNC(ir_arthsimp, ir_arth) {
  ir_cval_t cv;
  if ((cv = fold_to(v->in_map, arth2cval(n), 1, final ? -1 : n->lhs->id))) {
    ir_t *ir = cval2ir(cv, n->lhs);
    if (ir->irid != E_ir_arth || !same_ir_arth((ir_arth_t *)ir, n)) {
      do_opt = 1;
      v->ir_pos[0] = ir;
      return NULL;
    }
  }
  n->opr1 = try_fold(v->in_map, n->opr1, final ? -1 : n->lhs->id);
  n->opr2 = try_fold(v->in_map, n->opr2, final ? -1 : n->lhs->id);
  return NULL;
}

DEF_VISIT_FUNC(ir_arthsimp, ir_addr) {
  return NULL;
}

DEF_VISIT_FUNC(ir_arthsimp, ir_load) {
  n->rhs = (iropr_var_t *)try_fold(v->in_map, (iropr_t *)n->rhs, -1);
  assert(n->rhs->oprid == E_iropr_var);
  return NULL;
}

DEF_VISIT_FUNC(ir_arthsimp, ir_store) {
  n->lhs = (iropr_var_t *)try_fold(v->in_map, (iropr_t *)n->lhs, -1);
  assert(n->lhs->oprid == E_iropr_var);
  n->rhs = try_fold(v->in_map, n->rhs, -1);
  return NULL;
}

DEF_VISIT_FUNC(ir_arthsimp, ir_goto) {
  return NULL;
}

DEF_VISIT_FUNC(ir_arthsimp, ir_branch) {
  n->opr1 = try_fold(v->in_map, n->opr1, -1);
  n->opr2 = try_fold(v->in_map, n->opr2, -1);
  if (n->opr1->oprid == E_iropr_var && n->opr2->oprid == E_iropr_var) {
    iropr_var_t *v1 = (iropr_var_t *)(n->opr1), *v2 = (iropr_var_t *)(n->opr2);
    iropr_t *opr = 
      cval2opr(fold_to(v->in_map, make_vv(v1->id, v2->id, OP2_MINUS), 0, -1));
    if (opr) {
      n->opr1 = opr;
      n->opr2 = (iropr_t *)&IMM0;
      do_opt = 1;
    }
  }
  return NULL;
}

DEF_VISIT_FUNC(ir_arthsimp, ir_ret) {
  n->opr = try_fold(v->in_map, n->opr, -1);
  return NULL;
}

DEF_VISIT_FUNC(ir_arthsimp, ir_alloc) {
  return NULL;
}

DEF_VISIT_FUNC(ir_arthsimp, ir_call) {
  for (iroprs_t *l = n->args; l; l = l->next) {
    l->opr = try_fold(v->in_map, l->opr, -1);
  }
  return NULL;
}

DEF_VISIT_FUNC(ir_arthsimp, ir_read) {
  return NULL;
}

DEF_VISIT_FUNC(ir_arthsimp, ir_write) {
  n->opr = try_fold(v->in_map, n->opr, -1);
  return NULL;
}

#define IR_arthsimp_FUNC(name, ...) ir_arthsimp_##name,

static ir_visitor_table_t ir_arthsimp_table = {
  IRALL(IR_arthsimp_FUNC)
};

static void ir_arthsimp_bb(ir_cfg_t *cfg, ir_bb_t *bb) {
  ir_arthsimp_t visitor = {ir_arthsimp_table, NULL, NULL};
  ir_arthprog_res_t *res = &cfg->arthprog_res;
  LIST(ir_t*) *irs = cfg->irs;
  int st = bb->range.start, ed = bb->range.end - 1;
  for (int i = st; i <= ed; ++i) {
    visitor.in_map = res->res[i].in;
    visitor.out_map = res->res[i].out;
    visitor.ir_pos = (ir_t **)&(irs->array[i]);
    ir_visit(&visitor, irs->array[i]);
  }
}

int ir_arthprog(int final) {
  ir_program_t *program = get_ir_program();
  ir_arthprog_init(program, final);
  LIST(ir_cfg_t*) *cfgs = program->cfgs;
  for (int i = 0; i < cfgs->size; ++i) {
    ir_cfg_t *cfg = cfgs->array[i];
    if (!cfg->reachable) continue;
    ir_iter_cfg(cfg, &cfg->arthprog_res, cfg->arthprog_res.worklist,
      ir_arthprog_meet, ir_arthprog_skip, ir_arthprog_transfer_bb, 1);
    ir_analyse_cfg(cfg, ir_arthsimp_bb);
    build_cfg(cfg);
  }
  return do_opt;
}

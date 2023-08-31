#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "ir_visitor.h"
#include "ir.h"

static void ir_mips_init(ir_program_t *program) {
  LIST(ir_cfg_t*) *cfgs = program->cfgs;
  int vars = program->var_num;
  for (int i = 0; i < cfgs->size; ++i) {
    ir_cfg_t *cfg = cfgs->array[i];
    if (!cfg->reachable) continue;
    ir_mips_res_t *res = &cfg->mips_res;
    res->stack_size = 0;
    res->mem_var = calloc(vars, 4);
    memset(res->var_reg, 0xff, sizeof(res->var_reg));
    res->callee_saved = 0;
    res->dirty_var = new_bitset(vars, 0);
    res->cross_call = cfg->livevar_res.cross_call;
    res->mips = new_list();
    res->entry = new_list();
    res->exit = new_list();
  }
}

static void add_mips(ir_mips_res_t *res, void *mips) {
  list_append(res->mips, mips);
}

#define CALLER_NUM 15
#define CALLEE_NUM 8
#define UREG_NUM   23

static const mipsreg_t reg_caller[CALLER_NUM] = {
  R_A0, R_A1, R_A2, R_A3, R_V0, 
  R_T0, R_T1, R_T2, R_T3, R_T4, R_T5, R_T6, R_T7, R_T8, R_T9, 
};

static const mipsreg_t reg_callee[CALLEE_NUM] = {
  R_S0, R_S1, R_S2, R_S3, R_S4, R_S5, R_S6, R_S7,
};

static const mipsreg_t reg_canuse[UREG_NUM] = {
  R_A0, R_A1, R_A2, R_A3, R_V0, 
  R_T0, R_T1, R_T2, R_T3, R_T4, R_T5, R_T6, R_T7, R_T8, R_T9, 
  R_S0, R_S1, R_S2, R_S3, R_S4, R_S5, R_S6, R_S7,
};

static int create_stack(ir_mips_res_t *res, int n) {
  return (res->stack_size += n);
}

static int get_offset(ir_mips_res_t *res, int var_id) {
  if (res->mem_var[var_id] == 0) {
    res->mem_var[var_id] = create_stack(res, 4);
  }
  return res->mem_var[var_id];
}

static int find_var_reg(ir_mips_res_t *res, iropr_var_t *opr) {
  for (int i = 0; i < R_NUM; ++i) {
    if (res->var_reg[i] == opr->id) {
      return i;
    }
  }
  return -1;
}

static void write_back(ir_mips_res_t *res, mipsreg_t reg) {
  assert(res->var_reg[reg] >= 0);
  int var = res->var_reg[reg];
  if (bitset_test(res->dirty_var, var)) {
    add_mips(res, MIPSNEW(sw, MIPSSNEW(get_offset(res, var)), MIPSRNEW(reg)));
    bitset_clear(res->dirty_var, var);
  }
  res->var_reg[reg] = -1;
}

static void write_back_all(ir_mips_res_t *res) {
  for (int i = 0; i < R_NUM; ++i) {
    if (res->var_reg[i] >= 0) {
      write_back(res, i);
    }
  }
}

static void write_back_caller(ir_mips_res_t *res) {
  for (int i = 0; i < CALLER_NUM; ++i) {
    if (res->var_reg[reg_caller[i]] >= 0) {
      write_back(res, reg_caller[i]);
    }
  }
}

static int last_alloc_ureg_i = 0;

static int alloc_caller(ir_mips_res_t *res, iropr_var_t *opr) {
  for (int i = 0; i < CALLER_NUM; ++i) {
    if (res->var_reg[reg_caller[i]] < 0) {
      res->var_reg[reg_caller[i]] = opr->id;
      last_alloc_ureg_i = i;
      return reg_caller[i];
    }
  }
  return -1;
}

static int alloc_callee(ir_mips_res_t *res, iropr_var_t *opr) {
  for (int i = 0; i < CALLEE_NUM; ++i) {
    if (res->var_reg[reg_callee[i]] < 0) {
      res->callee_saved |= CALLEE_SAVED_MASK(reg_callee[i]);
      res->var_reg[reg_callee[i]] = opr->id;
      last_alloc_ureg_i = CALLER_NUM + i;
      return reg_callee[i];
    }
  }
  return -1;
}

static int alloc_reg(ir_mips_res_t *res, iropr_var_t *opr) {
  if (bitset_test(res->cross_call, opr->id)) {
    int i;
    if ((i = alloc_callee(res, opr)) > 0) return i;
    if ((i = alloc_caller(res, opr)) > 0) return i;
  } else {
    int i;
    if ((i = alloc_caller(res, opr)) > 0) return i;
    if ((i = alloc_callee(res, opr)) > 0) return i;
  }
  assert(res->callee_saved == 0xff);
  last_alloc_ureg_i = (last_alloc_ureg_i + 1) % UREG_NUM;
  write_back(res, reg_canuse[last_alloc_ureg_i]);
  res->var_reg[reg_canuse[last_alloc_ureg_i]] = opr->id;
  return reg_canuse[last_alloc_ureg_i];
}

static mipso_reg_t *get_rreg(ir_mips_res_t *res, iropr_t *opr) {
  if (opr->oprid == E_iropr_imm) {
    iropr_imm_t *imm = (iropr_imm_t *)opr;
    if (imm->val == 0) {
      return MIPSRNEW(R_ZERO);
    } else {
      add_mips(res, MIPSNEW(move, MIPSRNEW(R_V1), (mipso_t *)MIPSINEW(imm->val)));
      return MIPSRNEW(R_V1);
    }
  } else {
    assert(opr->oprid == E_iropr_var);
    iropr_var_t *var = (iropr_var_t *)opr;
    int reg = find_var_reg(res, var);
    if (reg > 0) {
      return MIPSRNEW(reg);
    } else {
      reg = alloc_reg(res, var);
      add_mips(res, MIPSNEW(lw, MIPSRNEW(reg), MIPSSNEW(get_offset(res, var->id))));
      assert(!bitset_test(res->dirty_var, var->id));
      return MIPSRNEW(reg);
    }
  }
}

static mipso_t *get_ropr(ir_mips_res_t *res, iropr_t *opr) {
  if (opr->oprid == E_iropr_imm) {
    iropr_imm_t *imm = (iropr_imm_t *)opr;
    return (mipso_t *)MIPSINEW(imm->val);
  } else {
    assert(opr->oprid == E_iropr_var);
    return (mipso_t *)get_rreg(res, opr);
  }
}

static mipso_reg_t *get_lreg(ir_mips_res_t *res, iropr_var_t *opr) {
  int reg = find_var_reg(res, opr);
  bitset_set(res->dirty_var, opr->id);
  if (reg > 0) {
    return MIPSRNEW(reg);
  } else {
    return MIPSRNEW(alloc_reg(res, opr));
  }
}

static void clean_reg(ir_mips_res_t *res, bitset_t *lvres) {
  for (int i = 0; i < R_NUM; ++i) {
    int var = res->var_reg[i];
    if (var >= 0 && !bitset_test(lvres, var)) {
      res->var_reg[i] = -1;
      bitset_clear(res->dirty_var, var);
    }
  }
}

static void pass_reg(ir_mips_res_t *res, iropr_t *opr, mipsreg_t reg) {
  if (opr->oprid == E_iropr_var) {
    iropr_var_t *var = (iropr_var_t *)opr;
    if (res->var_reg[reg] == var->id) {
      return;
    } else if (res->var_reg[reg] >= 0) {
      write_back(res, reg);
    }
    int reg2 = find_var_reg(res, var);
    if (reg2 > 0) {
      add_mips(res, MIPSNEW(move, MIPSRNEW(reg), (mipso_t *)MIPSRNEW(reg2)));
    } else {
      assert(!bitset_test(res->dirty_var, var->id));
      add_mips(res, MIPSNEW(lw, MIPSRNEW(reg), MIPSSNEW(get_offset(res, var->id))));
      res->var_reg[reg] = var->id;
    }
  } else {
    assert(opr->oprid == E_iropr_imm);
    iropr_imm_t *imm = (iropr_imm_t *)opr;
    if (res->var_reg[reg] >= 0) {
      write_back(res, reg);
    }
    add_mips(res, MIPSNEW(move, MIPSRNEW(reg), (mipso_t *)MIPSINEW(imm->val)));
  }
}

typedef struct ir_mips {
  void **table;
  ir_mips_res_t *res;
  ir_df_bs_t *lvres;
  int index, end;
} ir_mips_t;

DEF_VISIT_FUNC(ir_mips, ir_nop) {
  return NULL;
}

DEF_VISIT_FUNC(ir_mips, ir_label) {
  assert(0);
}

DEF_VISIT_FUNC(ir_mips, ir_func) {
  int i = -3;
  for (iropr_vars_t *l = n->params; l; (l = l->next), ++i) {
    if (i <= 0) {
      v->res->var_reg[R_A3 + i] = l->opr->id;
      bitset_set(v->res->dirty_var, l->opr->id);
    } else {
      v->res->mem_var[l->opr->id] = -(i * 4 + 4);
    }
  }
  return NULL;
}

DEF_VISIT_FUNC(ir_mips, ir_mov) {
  mipso_t *rhs = get_ropr(v->res, n->rhs);
  clean_reg(v->res, v->lvres->out);
  mipso_reg_t *lhs = get_lreg(v->res, n->lhs);
  if (rhs->oid == E_mipso_reg && ((mipso_reg_t *)rhs)->reg == lhs->reg) {
    return NULL;
  }
  add_mips(v->res, MIPSNEW(move, lhs, rhs));
  return NULL;
}

DEF_VISIT_FUNC(ir_mips, ir_arth) {
  mipso_reg_t *opr1 = get_rreg(v->res, n->opr1);
  mipso_t *opr2 = get_ropr(v->res, n->opr2);
  clean_reg(v->res, v->lvres->out);
  mipso_reg_t *lhs = get_lreg(v->res, n->lhs);
  add_mips(v->res, MIPSNEW(arth, lhs, opr1, opr2, n->op));
  return NULL;
}

DEF_VISIT_FUNC(ir_mips, ir_addr) {
  mipso_reg_t *lhs = get_lreg(v->res, n->lhs);
  int offset = v->res->mem_var[n->rhs->id];
  assert(offset);
  add_mips(v->res, 
    MIPSNEW(arth, lhs, MIPSRNEW(R_FP), (mipso_t *)MIPSINEW(-offset), OP2_PLUS));
  return NULL;
}

DEF_VISIT_FUNC(ir_mips, ir_load) {
  mipso_reg_t *rhs = get_rreg(v->res, (iropr_t *)n->rhs);
  clean_reg(v->res, v->lvres->out);
  mipso_reg_t *lhs = get_lreg(v->res, n->lhs);
  add_mips(v->res, MIPSNEW(lw, lhs, MIPSMNEW(rhs->reg, 0)));
  return NULL;
}

DEF_VISIT_FUNC(ir_mips, ir_store) {
  mipso_reg_t *rhs = get_rreg(v->res, n->rhs);
  mipso_reg_t *lhs = get_rreg(v->res, (iropr_t *)n->lhs);
  add_mips(v->res, MIPSNEW(sw, MIPSMNEW(lhs->reg, 0), rhs));
  return NULL;
}

DEF_VISIT_FUNC(ir_mips, ir_goto) {
  v->end = 1;
  write_back_all(v->res);
  add_mips(v->res, MIPSNEW(j, n->label->label));
  return NULL;
}

DEF_VISIT_FUNC(ir_mips, ir_branch) {
  v->end = 1;
  mipso_reg_t *opr1 = get_rreg(v->res, n->opr1);
  mipso_t *opr2 = get_ropr(v->res, n->opr2);
  clean_reg(v->res, v->lvres->out);
  write_back_all(v->res);
  add_mips(v->res, MIPSNEW(bcc, opr1, opr2, n->op, n->label->label));
  return NULL;
}

DEF_VISIT_FUNC(ir_mips, ir_ret) {
  v->end = 1;
  pass_reg(v->res, n->opr, R_V0);
  add_mips(v->res, MIPSNEW(ret));
  return NULL;
}

DEF_VISIT_FUNC(ir_mips, ir_alloc) {
  assert(v->res->mem_var[n->opr->id] == 0);
  v->res->mem_var[n->opr->id] = create_stack(v->res, n->size);
  return NULL;
}

DEF_VISIT_FUNC(ir_mips, ir_call) {
  int i = 0, args = 0;
  for (iroprs_t *l = n->args; l; l = l->next) args++;
  if (args > 4) {
    add_mips(v->res, 
      MIPSNEW(arth, MIPSRNEW(R_SP), MIPSRNEW(R_SP), 
        (mipso_t *)MIPSINEW(4 * (4 - args)), OP2_PLUS));
  }
  for (iroprs_t *l = n->args; l; (l = l->next), ++i) {
    iropr_t *opr = l->opr;
    if (i < 4) {
      pass_reg(v->res, opr, R_A0 + i);
    } else {
      int reg = R_V1;
      if (opr->oprid == E_iropr_var) {
        int reg2 = find_var_reg(v->res, (iropr_var_t *)opr);
        if (reg2 > 0) reg = reg2;
      }
      pass_reg(v->res, opr, reg);
      v->res->var_reg[R_V1] = -1;
      add_mips(v->res, MIPSNEW(sw, MIPSMNEW(R_SP, 4 * (i - 4)), MIPSRNEW(reg)));
    }
  }
  clean_reg(v->res, v->lvres->out);
  write_back_caller(v->res);
  add_mips(v->res, MIPSNEW(jal, n->func));
  if (args > 4) {
    add_mips(v->res, 
      MIPSNEW(arth, MIPSRNEW(R_SP), MIPSRNEW(R_SP), 
        (mipso_t *)MIPSINEW(4 * (args - 4)), OP2_PLUS));
  }
  v->res->var_reg[R_V0] = n->ret->id;
  bitset_set(v->res->dirty_var, n->ret->id);
  return NULL;
}

DEF_VISIT_FUNC(ir_mips, ir_read) {
  write_back_caller(v->res);
  add_mips(v->res, MIPSNEW(jal, strdup("read")));
  v->res->var_reg[R_V0] = n->opr->id;
  bitset_set(v->res->dirty_var, n->opr->id);
  return NULL;
}

DEF_VISIT_FUNC(ir_mips, ir_write) {
  pass_reg(v->res, n->opr, R_A0);
  clean_reg(v->res, v->lvres->out);
  write_back_caller(v->res);
  add_mips(v->res, MIPSNEW(jal, strdup("write")));
  return NULL;
}

#define IR_MIPS_FUNC(name, ...) ir_mips_##name,

static ir_visitor_table_t ir_mips_table = {
  IRALL(IR_MIPS_FUNC)
};

static void ir_mips_bb(ir_mips_res_t *res, ir_livevar_res_t *lvres, 
  LIST(ir_t*) *irs, ir_bb_t *bb) {
  ir_mips_t visitor = {ir_mips_table, res, NULL, 0, 0};
  int st = bb->range.start, ed = bb->range.end - 1;
  memset(res->var_reg, 0xff, sizeof(res->var_reg));
  bitset_zero(res->dirty_var);
  if (bb->id) {
    add_mips(res, MIPSNEW(label, bb->id->label));
  }
  for (int i = st; i <= ed; ++i) {
    assert(visitor.end == 0);
    visitor.lvres = &(lvres->res[i]);
    visitor.index = i;
    clean_reg(res, lvres->res[i].in);
    ir_visit(&visitor, irs->array[i]);
  }
  if (!visitor.end) {
    clean_reg(res, lvres->res[ed].out);
    write_back_all(res);
  }
}

static void ir_mips_cfg(ir_cfg_t *cfg) {
  ir_mips_res_t *res = &cfg->mips_res;
  ir_livevar_res_t *lvres = &cfg->livevar_res;
  LIST(ir_t*) *irs = cfg->irs;
  LIST(ir_bb_t*) *bbs = cfg->bbs;
  for (int i = 0; i < bbs->size; ++i) {
    ir_bb_t *curr = bbs->array[i];
    if (!curr->reachable) continue;
    ir_mips_bb(res, lvres, irs, curr);
  }
  int stack = res->stack_size;
  for (int i = R_S0; i <= R_S7; ++i) {
    if (res->callee_saved & CALLEE_SAVED_MASK(i)) {
      stack += 4;
    }
  }
  list_append(res->entry, MIPSNEW(func, cfg->name));
  list_append(res->entry, 
    MIPSNEW(arth, MIPSRNEW(R_SP), MIPSRNEW(R_SP), 
      (mipso_t *)MIPSINEW(-stack - 8), OP2_PLUS));
  list_append(res->entry, MIPSNEW(sw, MIPSMNEW(R_SP, stack + 4), MIPSRNEW(R_RA)));
  list_append(res->entry, MIPSNEW(sw, MIPSMNEW(R_SP, stack), MIPSRNEW(R_FP)));
  list_append(res->entry, 
    MIPSNEW(arth, MIPSRNEW(R_FP), MIPSRNEW(R_SP), 
      (mipso_t *)MIPSINEW(stack), OP2_PLUS));
  for (int i = R_S0, j = 0; i <= R_S7; ++i) {
    if (res->callee_saved & CALLEE_SAVED_MASK(i)) {
      list_append(res->entry, MIPSNEW(sw, MIPSMNEW(R_SP, j), MIPSRNEW(i)));
      list_append(res->exit, MIPSNEW(lw, MIPSRNEW(i), MIPSMNEW(R_SP, j)));
      j += 4;
    }
  }
  list_append(res->exit, MIPSNEW(arth, MIPSRNEW(R_SP), MIPSRNEW(R_FP), 
    (mipso_t *)MIPSINEW(8), OP2_PLUS));
  list_append(res->exit, MIPSNEW(lw, MIPSRNEW(R_RA), MIPSMNEW(R_FP, 4)));
  list_append(res->exit, MIPSNEW(lw, MIPSRNEW(R_FP), MIPSMNEW(R_FP, 0)));
}

void ir_mips() {
  ir_program_t *program = get_ir_program();
  ir_mips_init(program);
  LIST(ir_cfg_t*) *cfgs = program->cfgs;
  for (int i = 0; i < cfgs->size; ++i) {
    ir_cfg_t *cfg = cfgs->array[i];
    if (!cfg->reachable) continue;
    ir_mips_cfg(cfg);
  }
}

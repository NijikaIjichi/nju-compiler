#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "ir.h"

static ir_program_t *program;

int same_iropr(iropr_t *a, iropr_t *b) {
  if (a->oprid != b->oprid) return 0;
  if (a->oprid == E_iropr_var) {
    return ((iropr_var_t *)a)->id == ((iropr_var_t *)b)->id;
  } else {
    assert(a->oprid == E_iropr_imm);
    return ((iropr_imm_t *)a)->val == ((iropr_imm_t *)b)->val;
  }
}

uint64_t hash_iropr(iropr_t *a) {
  if (a->oprid == E_iropr_var) {
    return ((uint64_t)((iropr_var_t *)a)->id) << 1;
  } else {
    assert(a->oprid == E_iropr_imm);
    return (((uint64_t)((iropr_imm_t *)a)->val) << 1) | 1;
  }
}

int same_ir_arth(ir_arth_t *a, ir_arth_t *b) {
  if (a->op != b->op) return 0;
  int x = same_iropr(a->opr1, b->opr1) && same_iropr(a->opr2, b->opr2), y = 0;
  if (a->op == OP2_PLUS || a->op == OP2_STAR) {
    y = same_iropr(a->opr1, b->opr2) && same_iropr(a->opr2, b->opr1);
  }
  return x || y;
}

uint64_t hash_ir_arth(ir_arth_t *a) {
  uint64_t base;
  if (a->op == OP2_PLUS || a->op == OP2_STAR) {
    base = hash_iropr(a->opr1) ^ hash_iropr(a->opr2);
  } else {
    base = (hash_iropr(a->opr1) << 16) ^ hash_iropr(a->opr2);
  }
  return (base << 2) | (a->op & 3);
}

int is_iropr_imm(iropr_t *opr, int imm) {
  return opr->oprid == E_iropr_imm && ((iropr_imm_t *)opr)->val == imm;
}

ir_program_t *get_ir_program() {
  return program;
}

static uint64_t strhash(const char *s) {
  uint64_t hash = 0;
  for (; *s; ++s) {
    hash = hash * 31 + (uint64_t)(*s);
  }
  return hash;
}

static int strsame(const char *s1, const char *s2) {
  return !strcmp(s1, s2);
}

void init_ir_program() {
  assert(program == NULL);
  program = NEW(ir_program, new_list(), 0, 0, new_hmap(strsame, NULL, strhash, 0), NULL);
}

void add_cfg(ir_func_t *func) {
  ir_cfg_t *cfg = NEW(ir_cfg, func->func, program->cfgs->size, 1, 
    new_list(), new_list(), NULL, 
    new_hmap(same_ir_arth, same_iropr, hash_ir_arth, 0), NULL);
  list_append(cfg->irs, func);
  list_append(program->cfgs, cfg);
  hmap_put(program->func_table, func->func, cfg);
}

int new_var_id() {
  return program->var_num++;
}

iropr_var_t *gen_temp_var(type_t *type) {
  return IROPRNEW(iropr_var, program->var_num++, type);
}

ir_label_t *gen_label() {
  return IRNEW(ir_label, program->label_num++, 0, new_list());
}

void add_branch_goto(ir_t *ir) {
  if (ir->irid == E_ir_goto) {
    ir_goto_t *irg = (ir_goto_t *)ir;
    irg->label->ref++;
    list_append(irg->label->ins, irg);
  } else if (ir->irid == E_ir_branch) {
    ir_branch_t *irb = (ir_branch_t *)ir;
    irb->label->ref++;
    list_append(irb->label->ins, irb);
  } else assert(0);
}

void remove_branch_goto(ir_t *ir) {
  if (ir->irid == E_ir_goto) {
    ir_goto_t *irg = (ir_goto_t *)ir;
    irg->label->ref--;
    if (irg->label->ref == 0) {
      irg->label->irid = E_ir_nop;
    }
  } else if (ir->irid == E_ir_branch) {
    ir_branch_t *irb = (ir_branch_t *)ir;
    irb->label->ref--;
    if (irb->label->ref == 0) {
      irb->label->irid = E_ir_nop;
    }
  } else assert(0);
  ir->irid = E_ir_nop;
}

static void split_ir_arth(ir_cfg_t *cfg, ir_arth_t *ir) {
  iropr_var_t *lhs = ir->lhs, *tmp = hmap_get(cfg->expr_map, ir);
  if (tmp == NULL) {
    tmp = gen_temp_var(&INT);
    hmap_put(cfg->expr_map, ir, tmp);
  }
  ir->lhs = tmp;
  list_append(cfg->irs, ir);
  list_append(cfg->irs, IRNEW(ir_mov, lhs, (iropr_t *)tmp));
}

void add_ir(void *i) {
  ir_t *ir = i;
  ir_cfg_t *cfg = list_last(program->cfgs);
  if (ir->irid == E_ir_arth) {
    split_ir_arth(cfg, (ir_arth_t *)ir);
  } else {
    list_append(cfg->irs, ir);
    if (ir->irid == E_ir_goto || ir->irid == E_ir_branch) {
      add_branch_goto(ir);
    }
  }
}

void *ir_visit(void *visitor, void *ir) {
  if (!ir) return NULL;
  abstract_visitor_t *v = visitor;
  ir_t *n = ir;
  visitor_func_t *f = v->table[n->irid];
  return f(visitor, ir);
}

void ir_hole(void (*hole_func)(ir_t **), int n) {
  assert(n >= 1);
  LIST(ir_cfg_t*) *cfgs = program->cfgs;
  for (int i = 0; i < cfgs->size; ++i) {
    ir_cfg_t *cfg = cfgs->array[i];
    if (!cfg->reachable) continue;
    LIST(ir_t*) *irs = cfg->irs;
    for (int j = 0; j + n <= irs->size; ++j) {
      hole_func((ir_t **) &(irs->array[j]));
    }
  }
}

void check_program_reachable() {
  LIST(ir_cfg_t*) *cfgs = program->cfgs;
  for (int i = 0; i < cfgs->size; ++i) {
    ir_cfg_t *cfg = cfgs->array[i];
    cfg->reachable = 0;
  }
  if (program->worklist == NULL) {
    program->worklist = new_worklist(cfgs->size);
  } else {
    assert(worklist_empty(program->worklist) && program->worklist->size == cfgs->size);
  }
  worklist_t *worklist = program->worklist;
  ir_cfg_t *main = hmap_get(program->func_table, "main");
  if (!main) return;
  main->reachable = 1;
  worklist_add(worklist, main->no);
  while (!worklist_empty(worklist)) {
    int i = worklist_pop(worklist);
    ir_cfg_t *cfg = cfgs->array[i];
    assert(cfg->no == i);
    assert(cfg->reachable == 1);
    LIST(ir_t*) *irs = cfg->irs;
    for (int j = 0; j < irs->size; ++j) {
      ir_t *ir = irs->array[j];
      if (ir->irid == E_ir_call) {
        ir_call_t *call = (ir_call_t *)ir;
        ir_cfg_t *callee = hmap_get(program->func_table, call->func);
        assert(callee);
        if (callee->reachable == 0) {
          callee->reachable = 1;
          worklist_add(worklist, callee->no);
        }
      }
    }
  }
}

static void add_bb(LIST(ir_bb_t*) *bbs, ir_label_t *label, int start, int end) {
  ir_bb_t *bb = NEW(ir_bb, label, bbs->size, 
    RANGE(start, end), new_list(), new_list(), 0);
  list_append(bbs, bb);
  if (label) {
    label->bb = bb;
  }
}

static void build_bb(ir_cfg_t *cfg) {
  LIST(ir_t*) *irs = cfg->irs;
  LIST(ir_bb_t*) *bbs = cfg->bbs;
  ir_label_t *label = NULL;
  int start = 0;
  for (int j = 0; j < irs->size; ++j) {
    ir_t *ir = irs->array[j];
    switch (ir->irid) {
    case E_ir_branch:
    case E_ir_goto:
    case E_ir_ret:
      assert(start <= j);
      add_bb(bbs, label, start, j + 1);
      label = NULL;
      start = j + 1;
      break;
    case E_ir_label:
      if (start < j) {
        add_bb(bbs, label, start, j);
      }
      label = ((ir_label_t *)ir);
      start = j + 1;
      break;
    default: ;
    }
  }
  if (start < irs->size) {
    add_bb(bbs, label, start, irs->size);
    label = NULL;
  }
  assert(bbs->size > 0);
  cfg->exit = NEW(ir_bb, label, bbs->size, 
    RANGE(irs->size, irs->size), new_list(), new_list(), 0);
  if (label) {
    label->bb = cfg->exit;
  }
}

static void check_cfg_reachable(ir_cfg_t *cfg) {
  LIST(ir_bb_t*) *bbs = cfg->bbs;
  if (cfg->worklist == NULL) {
    cfg->worklist = new_worklist(bbs->size);
  } else {
    assert(worklist_empty(cfg->worklist) && cfg->worklist->size == bbs->size);
  }
  worklist_t *worklist = cfg->worklist;
  assert(bbs->size > 0);
  for (int i = 0; i < bbs->size; ++i) {
    ir_bb_t *bb = bbs->array[i];
    bb->reachable = 0;
  }
  ir_bb_t *entry = bbs->array[0];
  entry->reachable = 1;
  cfg->exit->reachable = 0;
  worklist_add(worklist, 0);
  while (!worklist_empty(worklist)) {
    int i = worklist_pop(worklist);
    ir_bb_t *bb = bbs->array[i];
    assert(bb->no == i);
    assert(bb->reachable);
    for (int j = 0; j < bb->outs->size; ++j) {
      ir_bb_t *out = bb->outs->array[j];
      if (out->reachable == 0) {
        out->reachable = 1;
        if (out != cfg->exit) {
          worklist_add(worklist, out->no);
        }
      }
    }
  }
  for (int i = 0; i < bbs->size; ++i) {
    ir_bb_t *bb = bbs->array[i];
    if (!bb->reachable) {
      int st = bb->range.start, ed = bb->range.end - 1;
      for (int i = st; i < ed; ++i) {
        ((ir_t *)(cfg->irs->array[i]))->irid = E_ir_nop;
      }
      ir_t *ir = cfg->irs->array[ed];
      if (ir->irid == E_ir_branch || ir->irid == E_ir_goto) {
        remove_branch_goto(ir);
      } else {
        ir->irid = E_ir_nop;
      }
    } else {
      for (int j = 0; j < bb->outs->size; ++j) {
        ir_bb_t *out = bb->outs->array[j];
        assert(out->reachable);
      }
    }
  }
}

void build_cfg(ir_cfg_t *cfg) {
  LIST(ir_bb_t*) *bbs = cfg->bbs;
  for (int i = 0; i < bbs->size; ++i) {
    ir_bb_t *bb = bbs->array[i];
    list_clear(bb->ins);
    list_clear(bb->outs);
  }
  list_clear(cfg->exit->ins);
  for (int i = 0; i < bbs->size; ++i) {
    ir_bb_t *bb = bbs->array[i];
    ir_t *last = cfg->irs->array[bb->range.end - 1];
    switch (last->irid) {
    case E_ir_branch: ;
      ir_branch_t *irb = (ir_branch_t *)last;
      ir_label_t *blabel = irb->label;
      assert(blabel->bb);
      list_append(bb->outs, blabel->bb);
      list_append(blabel->bb->ins, bb);
    default:
      if (i + 1 < bbs->size) {
        ir_bb_t *bbnext = bbs->array[i + 1];
        list_append(bb->outs, bbnext);
        list_append(bbnext->ins, bb);
      }
      break;
    case E_ir_goto: ;
      ir_goto_t *irg = (ir_goto_t *)last;
      ir_label_t *glabel = irg->label;
      assert(glabel->bb);
      list_append(bb->outs, glabel->bb);
      list_append(glabel->bb->ins, bb);
      break;
    case E_ir_ret: 
      list_append(bb->outs, cfg->exit);
      list_append(cfg->exit->ins, bb);
    }
  }
  check_cfg_reachable(cfg);
}

void build_program() {
  LIST(ir_cfg_t*) *cfgs = program->cfgs;
  for (int i = 0; i < cfgs->size; ++i) {
    ir_cfg_t *cfg = cfgs->array[i];
    build_bb(cfg);
    build_cfg(cfg);
  }
  check_program_reachable();
}

void ir_analyse_cfg(ir_cfg_t *cfg, void (*ana_func)(ir_cfg_t *, ir_bb_t *)) {
  LIST(ir_bb_t*) *bbs = cfg->bbs;
  for (int i = 0; i < bbs->size; ++i) {
    ir_bb_t *curr = bbs->array[i];
    if (!curr->reachable) continue;
    ana_func(cfg, curr);
  }
}

void ir_iter_cfg(ir_cfg_t *cfg, void *res, worklist_t *wl, 
  void *meet, void *skip, void *trans, int forward) {
  LIST(ir_t*) *irs = cfg->irs;
  LIST(ir_bb_t*) *bbs = cfg->bbs;
  void (*meetf)(void *, ir_bb_t *, ir_bb_t *) = meet;
  int (*skipf)(void *, ir_bb_t *) = skip;
  int (*transf)(void *, LIST(ir_t *) *, ir_bb_t *) = trans;
  assert(worklist_empty(wl));
  if (forward) {
    for (int i = 0; i < bbs->size; ++i) {
      ir_bb_t *curr = bbs->array[i];
      if (!curr->reachable) continue;
      worklist_add(wl, i);
    }
  } else {
    for (int i = bbs->size - 1; i >= 0; --i) {
      ir_bb_t *curr = bbs->array[i];
      if (!curr->reachable) continue;
      worklist_add(wl, i);
    }
  }
  while (!worklist_empty(wl)) {
    int i = worklist_pop(wl);
    ir_bb_t *curr = bbs->array[i];
    if (!curr->reachable) continue;
    LIST(ir_bb_t*) *outs, *ins;
    if (forward) {
      outs = curr->outs;
      ins = curr->ins;
    } else {
      outs = curr->ins;
      ins = curr->outs;
    }
    for (int j = 0; j < ins->size; ++j) {
      ir_bb_t *in = ins->array[j];
      if (!in->reachable) continue;
      meetf(res, curr, in);
    }
    if (skipf && skipf(res, curr)) {
      worklist_add(wl, i);
    } else if (transf(res, irs, curr)) {
      for (int j = 0; j < outs->size; ++j) {
        ir_bb_t *out = outs->array[j];
        if (!out->reachable || out == cfg->exit) continue;
        worklist_add(wl, out->no);
      }
    }
  }
}

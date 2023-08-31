#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "ir.h"

static const char start[] = 
  ".data\n"
  "_prompt: .asciiz \"Enter an integer:\"\n"
  "_ret: .asciiz \"\\n\"\n"
  ".globl main\n"
  ".text\n"
  "_read:\n"
  "li $v0, 4\n"
  "la $a0, _prompt\n"
  "syscall\n"
  "li $v0, 5\n"
  "syscall\n"
  "jr $ra\n"
  "_write:\n"
  "li $v0, 1\n"
  "syscall\n"
  "li $v0, 4\n"
  "la $a0, _ret\n"
  "syscall\n"
  "move $v0, $0\n"
  "jr $ra\n";

static const char *reg_name[] = {
  "zero", "at",
  "v0", "v1",
  "a0", "a1", "a2", "a3",
  "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
  "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
  "t8", "t9",
  "k0", "k1",
  "gp", "sp", "fp", "ra"
};

void *mips_visit(void *visitor, void *mips) {
  if (!mips) return NULL;
  abstract_visitor_t *v = visitor;
  mips_t *n = mips;
  visitor_func_t *f = v->table[n->mipsid];
  return f(visitor, mips);
}

typedef struct mips_dumper {
  void **table;
  FILE *fp;
  LIST(mips_t*) *exit;
} mips_dumper_t;

#define FPRINT(...) fprintf(v->fp, __VA_ARGS__)
#define FPUTREG(opr) FPRINT("$%s", reg_name[(opr)->reg])
#define FPUTIMM(opr) FPRINT("%d", (opr)->imm)
#define FPUTMEM(opr) FPRINT("%d($%s)", (opr)->offset, reg_name[(opr)->reg])
#define ISMAIN(s) !strcmp(s, "main")

static void dump_opr(mips_dumper_t *v, mipso_t *opr) {
  if (opr->oid == E_mipso_reg) {
    FPUTREG((mipso_reg_t *)opr);
  } else {
    assert(opr->oid == E_mipso_imm);
    FPUTIMM((mipso_imm_t *)opr);
  }
}

DEF_VISIT_FUNC(mips_dumper, mips_func) {
  if (ISMAIN(n->name)) {
    FPRINT("main:\n");
  } else {
    FPRINT("_%s:\n", n->name);
  }
  return NULL;
}

DEF_VISIT_FUNC(mips_dumper, mips_label) {
  FPRINT(".L%d:\n", n->label);
  return NULL;
}

DEF_VISIT_FUNC(mips_dumper, mips_arth) {
  switch (n->op) {
  case OP2_PLUS: FPRINT("addu "); break;
  case OP2_MINUS: FPRINT("subu "); break;
  case OP2_STAR: FPRINT("mul "); break;
  case OP2_DIV: FPRINT("div "); break;
  default: assert(0);
  }
  FPUTREG(n->lhs);
  FPRINT(", ");
  FPUTREG(n->opr1);
  FPRINT(", ");
  dump_opr(v, n->opr2);
  FPRINT("\n");
  return NULL;
}

DEF_VISIT_FUNC(mips_dumper, mips_move) {
  switch (n->rhs->oid) {
  case E_mipso_reg: FPRINT("move "); break;
  case E_mipso_imm: FPRINT("li "); break;
  default: assert(0);
  }
  FPUTREG(n->lhs);
  FPRINT(", ");
  dump_opr(v, n->rhs);
  FPRINT("\n");
  return NULL;
}

DEF_VISIT_FUNC(mips_dumper, mips_lw) {
  FPRINT("lw ");
  FPUTREG(n->lhs);
  FPRINT(", ");
  FPUTMEM(n->rhs);
  FPRINT("\n");
  return NULL;
}

DEF_VISIT_FUNC(mips_dumper, mips_sw) {
  FPRINT("sw ");
  FPUTREG(n->rhs);
  FPRINT(", ");
  FPUTMEM(n->lhs);
  FPRINT("\n");
  return NULL;
}

DEF_VISIT_FUNC(mips_dumper, mips_j) {
  FPRINT("j .L%d\n", n->label);
  return NULL;
}

DEF_VISIT_FUNC(mips_dumper, mips_jal) {
  if (ISMAIN(n->func)) {
    FPRINT("jal main\n");
  } else {
    FPRINT("jal _%s\n", n->func);
  }
  return NULL;
}

DEF_VISIT_FUNC(mips_dumper, mips_ret) {
  for (int i = 0; i < v->exit->size; ++i) {
    mips_visit(v, v->exit->array[i]);
  }
  FPRINT("jr $ra\n");
  return NULL;
}

DEF_VISIT_FUNC(mips_dumper, mips_bcc) {
  switch (n->op) {
  case GT: FPRINT("bgt "); break;
  case LE: FPRINT("ble "); break;
  case GE: FPRINT("bge "); break;
  case LT: FPRINT("blt "); break;
  case EQ: FPRINT("beq "); break;
  case NEQ: FPRINT("bne "); break;
  default: assert(0);
  }
  FPUTREG(n->opr1);
  FPRINT(", ");
  dump_opr(v, n->opr2);
  FPRINT(", .L%d\n", n->label);
  return NULL;
}

#define MIPS_DUMP_FUNC(name, ...) mips_dumper_##name,

static mips_visitor_table_t mips_dumper_table = {
  MIPSALL(MIPS_DUMP_FUNC)
};

int mips_dump(const char *file) {
  FILE *fp = fopen(file, "w");
  if (!fp) {
    perror(file);
    return 1;
  }
  ir_program_t *program = get_ir_program();
  mips_dumper_t visitor = {mips_dumper_table, fp, NULL};
  LIST(ir_cfg_t*) *cfgs = program->cfgs;
  fputs(start, fp);
  for (int i = 0; i < cfgs->size; ++i) {
    ir_cfg_t *cfg = cfgs->array[i];
    if (!cfg->reachable) continue;
    LIST(mips_t*) *entry = cfg->mips_res.entry, *mips = cfg->mips_res.mips;
    visitor.exit = cfg->mips_res.exit;
    for (int j = 0; j < entry->size; ++j) {
      mips_visit(&visitor, entry->array[j]);
    }
    for (int j = 0; j < mips->size; ++j) {
      mips_visit(&visitor, mips->array[j]);
    }
  }
  fclose(fp);
  return 0;
}

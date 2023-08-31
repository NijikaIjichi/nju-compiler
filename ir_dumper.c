#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "ir_visitor.h"
#include "ir.h"

typedef struct ir_dumper {
  void **table;
  FILE *fp;
} ir_dumper_t;

static void dump_opr(ir_dumper_t *v, iropr_t *opr) {
  if (opr->oprid == E_iropr_var) {
    fprintf(v->fp, "v%d", ((iropr_var_t*)opr)->id);
  } else {
    fprintf(v->fp, "#%d", ((iropr_imm_t*)opr)->val);
  }
}

DEF_VISIT_FUNC(ir_dumper, ir_nop) {
  return NULL;
}

DEF_VISIT_FUNC(ir_dumper, ir_label) {
  fprintf(v->fp, "LABEL .L%d :\n", n->label);
  return NULL;
}

DEF_VISIT_FUNC(ir_dumper, ir_func) {
  fprintf(v->fp, "FUNCTION %s :\n", n->func);
  for (iropr_vars_t *params = n->params; params; params = params->next) {
    fprintf(v->fp, "PARAM v%d\n", params->opr->id);
  }
  return NULL;
}

DEF_VISIT_FUNC(ir_dumper, ir_mov) {
  fprintf(v->fp, "v%d := ", n->lhs->id);
  dump_opr(v, n->rhs);
  fprintf(v->fp, "\n");
  return NULL;
}

DEF_VISIT_FUNC(ir_dumper, ir_arth) {
  fprintf(v->fp, "v%d := ", n->lhs->id);
  dump_opr(v, n->opr1);
  switch (n->op) {
  case OP2_PLUS: fprintf(v->fp, " + "); break;
  case OP2_MINUS: fprintf(v->fp, " - "); break;
  case OP2_STAR: fprintf(v->fp, " * "); break;
  case OP2_DIV: fprintf(v->fp, " / "); break;
  default: assert(0);
  }
  dump_opr(v, n->opr2);
  fprintf(v->fp, "\n");
  return NULL;
}

DEF_VISIT_FUNC(ir_dumper, ir_addr) {
  fprintf(v->fp, "v%d := &v%d\n", n->lhs->id, n->rhs->id);
  return NULL;
}

DEF_VISIT_FUNC(ir_dumper, ir_load) {
  fprintf(v->fp, "v%d := *v%d\n", n->lhs->id, n->rhs->id);
  return NULL;
}

DEF_VISIT_FUNC(ir_dumper, ir_store) {
  fprintf(v->fp, "*v%d := ", n->lhs->id);
  dump_opr(v, n->rhs);
  fprintf(v->fp, "\n");
  return NULL;
}

DEF_VISIT_FUNC(ir_dumper, ir_goto) {
  assert(n->label->irid == E_ir_label);
  fprintf(v->fp, "GOTO .L%d\n", n->label->label);
  return NULL;
}

DEF_VISIT_FUNC(ir_dumper, ir_branch) {
  fprintf(v->fp, "IF ");
  dump_opr(v, n->opr1);
  fprintf(v->fp, " %s ", ((char *[]){">", "<=", ">=", "<", "==", "!="})[n->op]);
  dump_opr(v, n->opr2);
  assert(n->label->irid == E_ir_label);
  fprintf(v->fp, " GOTO .L%d\n", n->label->label);
  return NULL;
}

DEF_VISIT_FUNC(ir_dumper, ir_ret) {
  fprintf(v->fp, "RETURN ");
  dump_opr(v, n->opr);
  fprintf(v->fp, "\n");
  return NULL;
}

DEF_VISIT_FUNC(ir_dumper, ir_alloc) {
  fprintf(v->fp, "DEC v%d %d\n", n->opr->id, n->size);
  return NULL;
}

static void dump_arg(ir_dumper_t *v, iroprs_t *args) {
  if (!args) return;
  dump_arg(v, args->next);
  fprintf(v->fp, "ARG ");
  dump_opr(v, args->opr);
  fprintf(v->fp, "\n");
}

DEF_VISIT_FUNC(ir_dumper, ir_call) {
  dump_arg(v, n->args);
  fprintf(v->fp, "v%d := CALL %s\n", n->ret->id, n->func);
  return NULL;
}

DEF_VISIT_FUNC(ir_dumper, ir_read) {
  fprintf(v->fp, "READ v%d\n", n->opr->id);
  return NULL;
}

DEF_VISIT_FUNC(ir_dumper, ir_write) {
  fprintf(v->fp, "WRITE ");
  dump_opr(v, n->opr);
  fprintf(v->fp, "\n");
  return NULL;
}

#define IR_DUMP_FUNC(name, ...) ir_dumper_##name,

static ir_visitor_table_t ir_dumper_table = {
  IRALL(IR_DUMP_FUNC)
};

static ir_dumper_t ir_dumper;

static void hole_dump_ir(ir_t *irs[1]) {
  ir_visit(&ir_dumper, irs[0]);
}

int ir_dump(const char *file) {
  FILE *fp = fopen(file, "w");
  if (!fp) {
    perror(file);
    return 1;
  }
  ir_dumper = (ir_dumper_t) {ir_dumper_table, fp};
  ir_hole(hole_dump_ir, 1);
  fclose(fp);
  return 0;
}

#ifndef __IR_H__
#define __IR_H__

#include "visitor.h"
#include "semantics.h"
#include "container.h"
#include "mips.h"
#include "ir_analyse.h"
#include <stdint.h>

typedef enum iropr_id { E_iropr_var, E_iropr_imm } iropr_id_t;

typedef struct iropr { iropr_id_t oprid; } iropr_t;
typedef struct iropr_var { iropr_id_t oprid; int id; type_t *type; } iropr_var_t;
typedef struct iropr_imm { iropr_id_t oprid; int val; } iropr_imm_t;

#define IROPRNEW(name, ...) NEW(name, E_##name, ##__VA_ARGS__)

extern iropr_imm_t IMM0, IMM1;

typedef struct iroprs { iropr_t *opr; struct iroprs *next; } iroprs_t;
typedef struct iropr_vars { iropr_var_t *opr; struct iropr_vars *next; } iropr_vars_t;

int same_iropr(iropr_t *a, iropr_t *b);
uint64_t hash_iropr(iropr_t *a);
int is_iropr_imm(iropr_t *opr, int imm);

struct ir_bb;

#define IRALL(_) \
  _(ir_nop) \
  _(ir_label, int label, ref; LIST(ir_t*) *ins; struct ir_bb *bb) \
  _(ir_func, char *func; iropr_vars_t *params) \
  _(ir_mov, iropr_var_t *lhs; iropr_t *rhs) \
  _(ir_arth, iropr_var_t *lhs; iropr_t *opr1, *opr2; op2_t op) \
  _(ir_addr, iropr_var_t *lhs, *rhs) \
  _(ir_load, iropr_var_t *lhs, *rhs) \
  _(ir_store, iropr_var_t *lhs; iropr_t *rhs) \
  _(ir_goto, ir_label_t *label) \
  _(ir_branch, iropr_t *opr1, *opr2; relop_t op; ir_label_t *label) \
  _(ir_ret, iropr_t *opr) \
  _(ir_alloc, iropr_var_t *opr; int size) \
  _(ir_call, iropr_var_t *ret; char *func; iroprs_t *args) \
  _(ir_read, iropr_var_t *opr) \
  _(ir_write, iropr_t *opr)

typedef enum { IRALL(DEF_ENUM) E_IRNUM } irid_t;

#define IR_DEF_STRUCT(name, ...) \
  typedef struct name { \
    irid_t irid; \
    __VA_ARGS__; \
  } name##_t;

IR_DEF_STRUCT(ir)
IRALL(IR_DEF_STRUCT)

#define IRNEW(name, ...) NEW(name, E_##name, ##__VA_ARGS__)

int same_ir_arth(ir_arth_t *a, ir_arth_t *b);
uint64_t hash_ir_arth(ir_arth_t *a);

typedef void *ir_visitor_table_t[E_IRNUM];
void *ir_visit(void *visitor, void *ir);

typedef struct ir_bb {
  ir_label_t *id;
  int no;
  range_t range;
  LIST(ir_bb_t*) *outs, *ins;
  int reachable;
} ir_bb_t;

typedef struct ir_cfg {
  char *name;
  int no, reachable;
  LIST(ir_t*) *irs;
  LIST(ir_bb_t*) *bbs;
  ir_bb_t *exit;
  HMAP(ir_arth_t *, iropr_var_t *) *expr_map;
  worklist_t *worklist;
  ir_livevar_res_t livevar_res;
  ir_avexpr_res_t avexpr_res;
  ir_constant_res_t constant_res;
  ir_arthprog_res_t arthprog_res;
  ir_mips_res_t mips_res;
} ir_cfg_t;

typedef struct ir_program {
  LIST(ir_cfg_t*) *cfgs;
  int var_num, label_num;
  HMAP(const char *, ir_cfg_t *) *func_table;
  worklist_t *worklist;
} ir_program_t;

ir_program_t *get_ir_program();
void init_ir_program();
void add_cfg(ir_func_t *func);
void add_ir(void *ir);
void add_branch_goto(ir_t *ir);
void remove_branch_goto(ir_t *ir);
int new_var_id();
iropr_var_t *gen_temp_var(type_t *type);
ir_label_t *gen_label();
void ir_hole(void (*hole_func)(ir_t **), int n);
void build_cfg(ir_cfg_t *cfg);
void build_program();
void check_program_reachable();
void ir_analyse_cfg(ir_cfg_t *cfg, void (*ana_func)(ir_cfg_t *, ir_bb_t *));
void ir_iter_cfg(ir_cfg_t *cfg, void *res, worklist_t *wl, 
  void *meet, // void meet(ir_res_t *res, ir_bb_t *dst, ir_bb_t *src);
  void *skip, // int skip(ir_res_t *res, ir_bb_t *bb); NULL = no skip
  void *trans, // int trans(ir_res_t *res, LIST(ir_t *) *irs, ir_bb_t *bb);
  int forward);

#endif

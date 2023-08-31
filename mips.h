#ifndef __MIPS_H__
#define __MIPS_H__

typedef enum mipsreg {
  R_ZERO, R_AT, 
  R_V0, R_V1, 
  R_A0, R_A1, R_A2, R_A3, 
  R_T0, R_T1, R_T2, R_T3, R_T4, R_T5, R_T6, R_T7, 
  R_S0, R_S1, R_S2, R_S3, R_S4, R_S5, R_S6, R_S7, 
  R_T8, R_T9, 
  R_K0, R_K1, 
  R_GP, R_SP, R_FP, R_RA, 
  R_NUM
} mipsreg_t;

#define IS_CALLEE_SAVED(reg)   ((reg) >= R_S0 && (reg) <= R_S7)
#define CALLEE_SAVED_MASK(reg) (1 << ((reg) - R_S0))

typedef enum mipso_id { E_mipso_reg, E_mipso_imm, E_mipso_mem } mipso_id_t;

typedef struct mipso { mipso_id_t oid; } mipso_t;
typedef struct mipso_reg { mipso_id_t oid; mipsreg_t reg; } mipso_reg_t;
typedef struct mipso_imm { mipso_id_t oid; int imm; } mipso_imm_t;
typedef struct mipso_mem { mipso_id_t oid; mipsreg_t reg; int offset; } mipso_mem_t;

#define MIPSONEW(name, ...) NEW(name, E_##name, ##__VA_ARGS__)
#define MIPSRNEW(reg) MIPSONEW(mipso_reg, reg)
#define MIPSINEW(imm) MIPSONEW(mipso_imm, imm)
#define MIPSMNEW(reg, offset) MIPSONEW(mipso_mem, reg, offset)
#define MIPSSNEW(offset) MIPSONEW(mipso_mem, R_FP, -(offset))

#define MIPSALL(_) \
  _(mips_func, char *name) \
  _(mips_label, int label) \
  _(mips_arth, mipso_reg_t *lhs, *opr1; mipso_t *opr2; op2_t op) \
  _(mips_move, mipso_reg_t *lhs; mipso_t *rhs) \
  _(mips_lw, mipso_reg_t *lhs; mipso_mem_t *rhs) \
  _(mips_sw, mipso_mem_t *lhs; mipso_reg_t *rhs) \
  _(mips_j, int label) \
  _(mips_jal, char *func) \
  _(mips_ret) \
  _(mips_bcc, mipso_reg_t *opr1; mipso_t *opr2; relop_t op; int label)

typedef enum { MIPSALL(DEF_ENUM) E_MIPSNUM } mipsid_t;

#define MIPS_DEF_STRUCT(name, ...) \
  typedef struct name { \
    mipsid_t mipsid; \
    __VA_ARGS__; \
  } name##_t;

MIPS_DEF_STRUCT(mips)
MIPSALL(MIPS_DEF_STRUCT)

#define MIPSNEW(name, ...) NEW(mips_##name, E_mips_##name, ##__VA_ARGS__)

typedef struct ir_mips_res {
  int stack_size;
  int *mem_var;
  int var_reg[R_NUM];
  int callee_saved;
  bitset_t *dirty_var, *cross_call;
  LIST(mips_t*) *mips, *entry, *exit;
} ir_mips_res_t;

typedef void *mips_visitor_table_t[E_MIPSNUM];
void *mips_visit(void *visitor, void *mips);

#endif

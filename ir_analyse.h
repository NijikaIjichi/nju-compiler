#ifndef __IR_ANALYSE_H__
#define __IR_ANALYSE_H__

#include "container.h"

typedef struct ir_df_bs {
  bitset_t *in, *out;
} ir_df_bs_t;

typedef struct ir_livevar_res {
  ir_df_bs_t *res;
  bitset_t *cross_call;
  worklist_t *worklist;
  bitset_t *buf;
} ir_livevar_res_t;

typedef struct ir_df_map {
  hashmap_t *in, *out;
} ir_df_map_t;

typedef struct ir_avexpr_res {
  ir_df_map_t *res;
  worklist_t *worklist;
  HMAP(ir_arth_t *, iropr_var_t *) *buf;
} ir_avexpr_res_t;

typedef void *ir_cval_t;

typedef struct ir_constant_res {
  ir_df_map_t *res;
  worklist_t *worklist;
  HMAP(iropr_var_t *, ir_cval_t) *buf;
} ir_constant_res_t;

typedef struct ir_arthprog_res {
  ir_df_map_t *res;
  worklist_t *worklist;
  HMAP(iropr_var_t *, ir_cval_t) *buf;
} ir_arthprog_res_t;

#define BB_OUT(r, bb) ((r)->res[(bb)->range.end - 1].out)
#define BB_IN(r, bb)  ((r)->res[(bb)->range.start].in)

#endif

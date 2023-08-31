#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "ir_visitor.h"
#include "ir.h"

static void hole_elim_jump1(ir_t *irs[2]) {
  if (irs[0]->irid == E_ir_goto && irs[1]->irid == E_ir_label) {
    ir_goto_t *ir0 = (ir_goto_t *)irs[0];
    ir_label_t *ir1 = (ir_label_t *)irs[1];
    if (ir0->label == ir1) {
      remove_branch_goto((ir_t *)ir0);
    }
  }
}

static void hole_elim_jump2(ir_t *irs[3]) {
  if (irs[0]->irid == E_ir_branch && irs[1]->irid == E_ir_goto 
      && irs[2]->irid == E_ir_label) {
    ir_branch_t *ir0 = (ir_branch_t *)irs[0];
    ir_goto_t *ir1 = (ir_goto_t *)irs[1];
    ir_label_t *ir2 = (ir_label_t *)irs[2];
    if (ir0->label == ir2) {
      ir_branch_t *ir0new = 
        IRNEW(ir_branch, ir0->opr1, ir0->opr2, ir0->op ^ 1, ir1->label);
      irs[0] = (ir_t *)ir0new;
      remove_branch_goto((ir_t *)ir0);
      add_branch_goto((ir_t *)ir0new);
      remove_branch_goto((ir_t *)ir1);
    }
  }
}

static void hole_elim_label(ir_t *irs[2]) {
  if (irs[0]->irid == E_ir_label && irs[1]->irid == E_ir_label) {
    ir_label_t *ir0 = (ir_label_t *)irs[0];
    ir_label_t *ir1 = (ir_label_t *)irs[1];
    for (int i = 0; i < ir0->ins->size; i++) {
      ir_t *ir = ir0->ins->array[i];
      if (ir->irid == E_ir_goto) {
        ir_goto_t *irg = (ir_goto_t *)ir;
        assert(irg->label == ir0);
        irg->label = ir1;
        ir1->ref++;
        list_append(ir1->ins, irg);
      } else if (ir->irid == E_ir_branch) {
        ir_branch_t *irb = (ir_branch_t *)ir;
        assert(irb->label == ir0);
        irb->label = ir1;
        ir1->ref++;
        list_append(ir1->ins, irb);
      } else {
        assert(ir->irid == E_ir_nop);
      }
    }
    ir0->irid = E_ir_nop;
  }
}

static void hole_constant(ir_t *irs[1]) {
  ir_t *ir = irs[0];
  if (ir->irid == E_ir_arth) {
    ir_arth_t *arth = (ir_arth_t *)ir;
    if (arth->opr1->oprid == E_iropr_imm && arth->opr2->oprid == E_iropr_imm) {
      int opr1 = ((iropr_imm_t *)(arth->opr1))->val, 
          opr2 = ((iropr_imm_t *)(arth->opr2))->val, res;
      switch (arth->op) {
      case OP2_PLUS: res = opr1 + opr2; break;
      case OP2_MINUS: res = opr1 - opr2; break;
      case OP2_STAR: res = opr1 * opr2; break;
      case OP2_DIV: res = opr2 == 0 ? 0 : opr1 / opr2; break;
      default: assert(0);
      }
      irs[0] = (ir_t *)IRNEW(ir_mov, arth->lhs, (void*)IROPRNEW(iropr_imm, res));
    } else if (arth->op == OP2_PLUS || arth->op == OP2_STAR) {
      if (arth->opr1->oprid == E_iropr_imm) {
        iropr_t *opr = arth->opr1;
        arth->opr1 = arth->opr2;
        arth->opr2 = opr;
      }
      if (arth->op == OP2_STAR && same_iropr(arth->opr2, (iropr_t *)&IMM0)) {
        irs[0] = (ir_t *)IRNEW(ir_mov, arth->lhs, (iropr_t *)&IMM0);
      }
    } else if (arth->op == OP2_MINUS && same_iropr(arth->opr1, arth->opr2)) {
      irs[0] = (ir_t *)IRNEW(ir_mov, arth->lhs, (iropr_t *)&IMM0);
    } else if (arth->op == OP2_DIV) {
      if (same_iropr(arth->opr1, arth->opr2)) {
        irs[0] = (ir_t *)IRNEW(ir_mov, arth->lhs, (iropr_t *)&IMM1);
      } else if (same_iropr(arth->opr2, (iropr_t *)&IMM0)) {
        irs[0] = (ir_t *)IRNEW(ir_mov, arth->lhs, (iropr_t *)&IMM0);
      }
    }
  } else if (ir->irid == E_ir_branch) {
    ir_branch_t *branch = (ir_branch_t *)ir;
    if (branch->opr1->oprid == E_iropr_imm && branch->opr2->oprid == E_iropr_imm) {
      int opr1 = ((iropr_imm_t *)(branch->opr1))->val, 
          opr2 = ((iropr_imm_t *)(branch->opr2))->val, res;
      switch (branch->op) {
      case GT: res = opr1 > opr2; break;
      case LE: res = opr1 <= opr2; break;
      case GE: res = opr1 >= opr2; break;
      case LT: res = opr1 < opr2; break;
      case EQ: res = opr1 == opr2; break;
      case NEQ: res = opr1 != opr2; break;
      default: assert(0);
      }
      if (res) {
        ir_goto_t *newir = IRNEW(ir_goto, branch->label);
        add_branch_goto((ir_t *)newir);
        irs[0] = (ir_t *)newir;
      }
      remove_branch_goto((ir_t *)ir);
    } else if (branch->opr1->oprid == E_iropr_imm) {
      iropr_t *opr = branch->opr1;
      branch->opr1 = branch->opr2;
      branch->opr2 = opr;
      if (branch->op <= 3) {
        branch->op = 3 - branch->op;
      }
    } else if (same_iropr(branch->opr1, branch->opr2)) {
      switch (branch->op) {
      case LE:
      case GE:
      case EQ: ;
        ir_goto_t *newir = IRNEW(ir_goto, branch->label);
        add_branch_goto((ir_t *)newir);
        irs[0] = (ir_t *)newir;
      default:
        remove_branch_goto((ir_t *)ir);
      }
    }
  }
}

void ir_hole_opt() {
  ir_hole(hole_constant, 1);
  ir_hole(hole_elim_jump1, 2);
  ir_hole(hole_elim_jump2, 3);
  ir_hole(hole_elim_label, 2);
}



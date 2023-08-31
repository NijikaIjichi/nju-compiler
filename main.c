#include <stdio.h>
#include "ast_visitor.h"
#include "ir_visitor.h"
#include "mips_visitor.h"

extern FILE *yyin;
int yyparse();

static int error = 0;

#define WAIT() //({if (argc > 3) {ir_dump(argv[3]);} putchar('\n'); getchar();})

int main(int argc, char** argv) {
  if (argc < 3) {
    return 1;
  }
  if (!(yyin = fopen(argv[1], "r"))) {
    perror(argv[1]);
    return 1;
  }
  yyparse();
  if (error) return 0;
  ast_ir();
  ir_hole_opt();
  if (argc > 4) ir_dump(argv[4]);
  build_program();
  WAIT();
  while (ir_constant() | ir_livevar(0) | ir_arthprog(0) | ir_avexpr(0)) WAIT();
  while (ir_constant() | ir_arthprog(1) | ir_livevar(1)) WAIT();
  while (ir_constant() | ir_avexpr(1) | ir_livevar(1)) WAIT();
  return ir_dump(argv[2]);
}

void set_error() {
  error = 1;
}

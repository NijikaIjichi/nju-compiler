#include "ast.h"
#include <stdlib.h>

static program_t *root;

void ast_set_root(program_t *node) {
  root = node;
}

program_t *ast_get_root() {
  return root;
}

void *ast_visit(void *visitor, void *node) {
  if (!node) return NULL;
  abstract_visitor_t *v = visitor;
  abstract_node_t *n = node;
  visitor_func_t *f = v->table[n->astid];
  return f(visitor, node);
}

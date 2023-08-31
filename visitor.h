#ifndef __VISITOR_H__
#define __VISITOR_H__

typedef void *visitor_func_t(void *visitor, void *node);
typedef struct abstract_visitor {
  void **table;
} abstract_visitor_t;

#define DEF_VISIT_FUNC(visit, name) \
  static void *visit##_##name(visit##_t *v, name##_t *n)

#endif

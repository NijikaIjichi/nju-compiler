#ifndef __TYPE_H__
#define __TYPE_H__

#include <stdint.h>

char *strdup(const char *);
uint64_t strhash(const char *s);
int strsame(const char *s1, const char *s2);

typedef enum typeid {E_type_int, E_type_float, E_type_array, E_type_struct, E_type_func, E_type_ref, E_type_error} typeid_t;
typedef enum relop {GT, LE, GE, LT, EQ, NEQ} relop_t;
typedef enum op2 {OP2_ASSIGNOP, OP2_AND, OP2_OR, OP2_RELOP, OP2_PLUS, OP2_MINUS, OP2_STAR, OP2_DIV} op2_t;
typedef enum op1 {OP1_MINUS, OP1_NOT} op1_t;

#define DEF_ENUM(name, ...) E_##name,

#define NEW(name, ...) ({ \
  name##_t *__new_obj = malloc(sizeof(name##_t)); \
  *__new_obj = (name##_t) { __VA_ARGS__ }; \
  __new_obj; })

#define TYPENEW(name, ...) NEW(name, E_##name, ##__VA_ARGS__)

#define MIN(a, b) (((a) > (b)) ? (b) : (a))
#define MAX(a, b) (((a) < (b)) ? (b) : (a))

typedef struct range { int start, end; } range_t;
#define RANGE(st, ed) ((range_t){st, ed})

#endif

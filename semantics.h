#ifndef __SEMANTICS_H__
#define __SEMANTICS_H__

#include "type.h"

struct type;

typedef struct var {
  struct type *type;
  char *name;
  int offset;
  int id;
} var_t;

typedef struct vars {
  var_t *var;
  struct vars *next;
} vars_t;

typedef struct frame {
  vars_t *var_list;
  struct frame *parent;
  int size;
} frame_t;

struct struct_;

typedef struct type {
  typeid_t typeid;
  int size;
} type_t;

typedef struct type_array {
  typeid_t typeid;
  int size;
  int num;
  type_t *next;
} type_array_t;

typedef struct type_struct {
  typeid_t typeid;
  int size;
  struct struct_ *struct_;
} type_struct_t;

typedef struct type_func {
  typeid_t typeid;
  int size;
  vars_t *arg_list;
  type_t *ret;
} type_func_t;

typedef struct type_ref {
  typeid_t typeid;
  int size;
  type_t *ref;
} type_ref_t;

var_t *lookup_var_deep(frame_t *frame, const char *name);

typedef struct struct_ {
  char *name;
  struct frame *fields;
} struct__t;

extern type_t INT, FLOAT, ERROR;

typedef struct structs {
  struct__t *struct_;
  struct structs *next;
} structs_t;

structs_t *get_struct_list();

#endif


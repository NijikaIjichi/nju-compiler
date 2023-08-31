#ifndef __CONTAINER_H__
#define __CONTAINER_H__

#include <stdint.h>

typedef struct list {
  void **array;
  int size, cap;
} list_t;

#define LIST(T) list_t

list_t *new_list();
void list_append(list_t *lst, void *elem);
void list_clear(list_t *lst);
void *list_last(list_t *lst);

typedef struct bitset {
  uint64_t *array;
  int size;
} bitset_t;

bitset_t *new_bitset(int n, int i);
int bitset_test(bitset_t *bs, int n);
void bitset_set(bitset_t *bs, int n);
void bitset_clear(bitset_t *bs, int n);
void bitset_zero(bitset_t *bs);
void bitset_one(bitset_t *bs);
void bitset_copy(bitset_t *dst, bitset_t *src);
void bitset_and(bitset_t *dst, bitset_t *src);
void bitset_or(bitset_t *dst, bitset_t *src);
int bitset_cmp(bitset_t *dst, bitset_t *src);

typedef struct worklist {
  int *lst;
  uint32_t start, end, size, cap;
  bitset_t *is_in;
} worklist_t;

worklist_t *new_worklist(int n);
void worklist_add(worklist_t *wl, int x);
int worklist_empty(worklist_t *wl);
int worklist_pop(worklist_t *wl);

typedef struct node {
  void *key, *value;
  struct node *prev, *next;
} node_t;

typedef struct map {
  node_t node;
  int (*keqfunc)(void *, void *);
  int (*veqfunc)(void *, void *);
  int size;
} map_t;

#define MAP(K, V) map_t
#define ANY ((void *)1)

map_t *new_map(void *keqfunc, // int (*keqfunc)(K, K);
  void *veqfunc // int (*veqfunc)(V, V);
  );
int map_put(map_t *map, void *key, void *value);
void *map_get(map_t *map, void *key);
void *map_remove(map_t *map, void *key);
int map_removeif(map_t *map, 
  void *condfunc // int (*condfunc)(K, V);
  );
void map_removeall(map_t *map);
void map_combine(map_t *dst, map_t *src, 
  void *combfunc // V (*combfunc)(K, V, V);
  );
void map_copy(map_t *dst, map_t *src);
void map_and(map_t *dst, map_t *src);
int map_cmp(map_t *m1, map_t *m2);

typedef struct hashmap {
  map_t **buckets;
  int size, bknum;
  int (*keqfunc)(void *, void *);
  int (*veqfunc)(void *, void *);
  uint64_t (*hashfunc)(void *);
  int is_top;
} hashmap_t;

#define HMAP(K, V) hashmap_t

hashmap_t *new_hmap(void *keqfunc, // int (*keqfunc)(K, K);
  void *veqfunc, // int (*veqfunc)(V, V);
  void *hashfunc, // uint64_t hashfunc(K);
  int is_top);
void hmap_put(hashmap_t *map, void *key, void *value);
void *hmap_get(hashmap_t *map, void *key);
void *hmap_remove(hashmap_t *map, void *key);
int hmap_removeif(hashmap_t *map, 
  void *condfunc // int (*condfunc)(K, V);
  );
void hmap_removeall(hashmap_t *map);
void hmap_combine(hashmap_t *dst, hashmap_t *src, 
  void *combfunc // V (*combfunc)(K, V, V);
  );
void hmap_copy(hashmap_t *dst, hashmap_t *src);
void hmap_and(hashmap_t *dst, hashmap_t *src);
int hmap_cmp(hashmap_t *m1, hashmap_t *m2);


#endif

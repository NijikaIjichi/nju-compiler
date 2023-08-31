#include "container.h"
#include "type.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

list_t *new_list() {
  return NEW(list, malloc(8 * sizeof(void*)), 0, 8);
}

void list_append(list_t *lst, void *elem) {
  if (lst->cap == lst->size) {
    lst->array = realloc(lst->array, lst->cap * 2 * sizeof(void*));
    lst->cap *= 2;
  }
  lst->array[lst->size++] = elem;
}

void list_clear(list_t *lst) {
  lst->size = 0;
}

void *list_last(list_t *lst) {
  assert(lst->size > 0);
  return lst->array[lst->size - 1];
}

#define MASK(n) (1ul << (n))

bitset_t *new_bitset(int n, int i) {
  int size = (n + 63) / 64;
  bitset_t *bitset = NEW(bitset, calloc(size, 8), size);
  if (i) bitset_one(bitset);
  return bitset;
}

int bitset_test(bitset_t *bs, int n) {
  assert(n >= 0 && n < bs->size * 64);
  return !!(bs->array[n / 64] & MASK(n % 64));
}

void bitset_set(bitset_t *bs, int n) {
  assert(n >= 0 && n < bs->size * 64);
  bs->array[n / 64] |= MASK(n % 64);
}

void bitset_clear(bitset_t *bs, int n) {
  assert(n >= 0 && n < bs->size * 64);
  bs->array[n / 64] &= ~MASK(n % 64);
}

void bitset_zero(bitset_t *bs) {
  memset(bs->array, 0, bs->size * 8);
}

void bitset_one(bitset_t *bs) {
  memset(bs->array, 0xff, bs->size * 8);
}

void bitset_copy(bitset_t *dst, bitset_t *src) {
  assert(dst->size == src->size);
  memcpy(dst->array, src->array, dst->size * 8);
}

void bitset_and(bitset_t *dst, bitset_t *src) {
  assert(dst->size == src->size);
  for (int i = 0; i < dst->size; ++i) {
    dst->array[i] &= src->array[i];
  }
}

void bitset_or(bitset_t *dst, bitset_t *src) {
  assert(dst->size == src->size);
  for (int i = 0; i < dst->size; ++i) {
    dst->array[i] |= src->array[i];
  }
}

int bitset_cmp(bitset_t *dst, bitset_t *src) {
  assert(dst->size == src->size);
  return !!memcmp(dst->array, src->array, dst->size * 8);
}

static int round2power(int x) {
  assert(x > 0);
  x -= 1;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return x + 1;
}

worklist_t *new_worklist(int n) {
  int m = round2power(n);
  assert(m >= n && (m & (m - 1)) == 0);
  return NEW(worklist, calloc(m, 4), 0, 0, n, m, new_bitset(n, 0));
}

void worklist_add(worklist_t *wl, int x) {
  assert(x >= 0 && x < wl->size && wl->end != wl->start + wl->size);
  if (!bitset_test(wl->is_in, x)) {
    bitset_set(wl->is_in, x);
    wl->lst[wl->end++ & (wl->cap - 1)] = x;
  }
}

int worklist_empty(worklist_t *wl) {
  return wl->start == wl->end;
}

int worklist_pop(worklist_t *wl) {
  assert(!worklist_empty(wl));
  int x = wl->lst[wl->start++ & (wl->cap - 1)];
  bitset_clear(wl->is_in, x);
  return x;
}

static int default_eqfunc(void *a, void *b) {
  return a == b;
}

map_t *new_map(void *keqfunc, void *veqfunc) {
  if (keqfunc == NULL) keqfunc = default_eqfunc;
  if (veqfunc == NULL) veqfunc = default_eqfunc;
  map_t *map = NEW(map, (node_t){}, keqfunc, veqfunc, 0);
  map->node.prev = map->node.next = &(map->node);
  return map;
}

static node_t *map_find(map_t *map, void *key) {
  node_t *n = &(map->node);
  for (node_t *l = n->next; l != n; l = l->next) {
    if (map->keqfunc(key, l->key)) {
      return l;
    }
  }
  return NULL;
}

static void map_insert(map_t *map, node_t *node) {
  node->prev = &(map->node);
  node->next = map->node.next;
  node->prev->next = node->next->prev = node;
  map->size += 1;
}

static void map_delete(map_t *map, node_t *n) {
  n->prev->next = n->next;
  n->next->prev = n->prev;
  map->size -= 1;
}

int map_put(map_t *map, void *key, void *value) {
  if (value == NULL) {
    map_remove(map, key);
    return -1;
  } else {
    node_t *n = map_find(map, key);
    if (n) {
      n->value = value;
      return 0;
    } else {
      map_insert(map, NEW(node, key, value));
      return 1;
    }
  }
}

void *map_get(map_t *map, void *key) {
  node_t *n = map_find(map, key);
  if (n) return n->value;
  return NULL;
}

void *map_remove(map_t *map, void *key) {
  node_t *n = map_find(map, key);
  if (n) {
    map_delete(map, n);
    return n->value;
  }
  return NULL;
}

int map_removeif(map_t *map, void *condfunc) {
  node_t *n = &(map->node);
  int removed = 0;
  int (*cond)(void *, void *) = condfunc;
  for (node_t *l = n->next; l != n; l = l->next) {
    if (cond(l->key, l->value)) {
      map_delete(map, l);
      removed += 1;
    }
  }
  return removed;
}

void map_removeall(map_t *map) {
  map->node.prev = map->node.next = &(map->node);
  map->size = 0;
}

void map_combine(map_t *dst, map_t *src, void *combfunc) {
  void *(*comb)(void *, void *, void *, void *) = combfunc;
  assert(dst->keqfunc == src->keqfunc && dst->veqfunc == src->veqfunc);
  for (node_t *l = dst->node.next; l != &(dst->node); l = l->next) {
    void *v = comb(l->key, l->value, map_get(src, l->key), dst->veqfunc);
    if (v == NULL) map_delete(dst, l);
    else l->value = v;
  }
  for (node_t *l = src->node.next; l != &(src->node); l = l->next) {
    void *dv = map_get(dst, l->key);
    if (dv == NULL) {
      void *v = comb(l->key, NULL, l->value, dst->veqfunc);
      if (v) map_put(dst, l->key, v);
    }
  }
}

void map_copy(map_t *dst, map_t *src) {
  map_removeall(dst);
  for (node_t *l = src->node.next; l != &(src->node); l = l->next) {
    map_put(dst, l->key, l->value);
  }
}

static void *map_and_comb(void *k, void *v1, void *v2, int (*veq)(void *, void *)) {
  if (v1 == NULL || v2 == NULL) {
    return NULL;
  } else {
    return veq(v1, v2) ? v1 : NULL;
  }
}

void map_and(map_t *dst, map_t *src) {
  map_combine(dst, src, map_and_comb);
}

int map_cmp(map_t *m1, map_t *m2) {
  if (m1->size != m2->size) {
    return 1;
  } else {
    for (node_t *l = m1->node.next; l != &(m1->node); l = l->next) {
      void *v2 = map_get(m2, l->key);
      if (!v2 || !m1->veqfunc(l->value, v2)) return 1;
    }
    return 0;
  }
}

static uint64_t default_hash(void *k) {
  return (uint64_t)k;
}

hashmap_t *new_hmap(void *keqfunc, void *veqfunc, void *hashfunc, int is_top) {
  if (keqfunc == NULL) keqfunc = default_eqfunc;
  if (veqfunc == NULL) veqfunc = default_eqfunc;
  if (hashfunc == NULL) hashfunc = default_hash;
  hashmap_t *map = NEW(hashmap, calloc(8, sizeof(map_t*)), 0, 8, 
    keqfunc, veqfunc, hashfunc, is_top);
  for (int i = 0; i < 8; ++i) {
    map->buckets[i] = new_map(keqfunc, veqfunc);
  }
  return map;
}

static int hash2bk(uint64_t hash, int bknum) {
  hash ^= (hash >> 20) ^ (hash >> 12);
  return (int)((hash ^ (hash >> 4) ^ (hash >> 7)) & (bknum - 1));
}

static void hmap_rehash(hashmap_t *map, int target) {
  int bknum = map->bknum;
  if (target == 0) {
    if (map->size * 4 < bknum * 3) return;
    target = bknum * 2;
  } else {
    assert(target >= bknum * 2);
  }
  map->bknum = target;
  map->buckets = realloc(map->buckets, target * sizeof(map_t*));
  for (int i = bknum; i < target; ++i) {
    map->buckets[i] = new_map(map->keqfunc, map->veqfunc);
  }
  for (int i = 0; i < bknum; ++i) {
    map_t *m = map->buckets[i];
    for (node_t *l = m->node.next, *n = l->next; 
        l != &(m->node); (l = n), (n = n->next)) {
      int j = hash2bk(map->hashfunc(l->key), target);
      if (j != i) {
        map_delete(m, l);
        map_insert(map->buckets[j], l);
      }
    }
  }
}

void hmap_put(hashmap_t *map, void *key, void *value) {
  assert(!map->is_top);
  int bkno = hash2bk(map->hashfunc(key), map->bknum);
  map->size += map_put(map->buckets[bkno], key, value);
  hmap_rehash(map, 0);
}

void *hmap_get(hashmap_t *map, void *key) {
  if (map->is_top) return ANY;
  int bkno = hash2bk(map->hashfunc(key), map->bknum);
  return map_get(map->buckets[bkno], key);
}

void *hmap_remove(hashmap_t *map, void *key) {
  assert(!map->is_top);
  int bkno = hash2bk(map->hashfunc(key), map->bknum);
  void *v = map_remove(map->buckets[bkno], key);
  if (v) map->size -= 1;
  return v;
}

int hmap_removeif(hashmap_t *map, void *condfunc) {
  assert(!map->is_top);
  int removed = 0;
  for (int i = 0; i < map->bknum; ++i) {
    removed += map_removeif(map->buckets[i], condfunc);
  }
  map->size -= removed;
  return removed;
}

void hmap_removeall(hashmap_t *map) {
  assert(!map->is_top);
  for (int i = 0; i < map->bknum; ++i) {
    map_removeall(map->buckets[i]);
  }
  map->size = 0;
}

static void hmap_recount(hashmap_t *map) {
  int size = 0;
  for (int i = 0; i < map->bknum; ++i) {
    size += map->buckets[i]->size;
  }
  map->size = size;
  hmap_rehash(map, 0);
}

static void hmap_rehash_like(hashmap_t *m1, hashmap_t *m2) {
  assert(!m1->is_top && !m2->is_top);
  if (m1->bknum < m2->bknum) {
    hmap_rehash(m1, m2->bknum);
  } else if (m1->bknum > m2->bknum) {
    hmap_rehash(m2, m1->bknum);
  }
  assert(m1->bknum == m2->bknum);
}

void hmap_combine(hashmap_t *dst, hashmap_t *src, void *combfunc) {
  hmap_rehash_like(dst, src);
  for (int i = 0; i < dst->bknum; ++i) {
    map_combine(dst->buckets[i], src->buckets[i], combfunc);
  }
  hmap_recount(dst);
}

void hmap_copy(hashmap_t *dst, hashmap_t *src) {
  if (src->is_top) {
    if (!dst->is_top) {
      hmap_removeall(dst);
      dst->is_top = 1;
    }
  } else {
    dst->is_top = 0;
    hmap_removeall(dst);
    hmap_rehash_like(dst, src);
    for (int i = 0; i < dst->bknum; ++i) {
      map_copy(dst->buckets[i], src->buckets[i]);
    }
    hmap_recount(dst);
  }
}

void hmap_and(hashmap_t *dst, hashmap_t *src) {
  if (src->is_top) return;
  else if (dst->is_top) hmap_copy(dst, src);
  else {
    hmap_combine(dst, src, map_and_comb);
  }
}

int hmap_cmp(hashmap_t *m1, hashmap_t *m2) {
  if (m1->is_top || m2->is_top) return m1->is_top != m2->is_top;
  if (m1->size != m2->size) return 1;
  hmap_rehash_like(m1, m2);
  for (int i = 0; i < m1->bknum; ++i) {
    if (map_cmp(m1->buckets[i], m2->buckets[i])) {
      return 1;
    }
  }
  return 0;
}

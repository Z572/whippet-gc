#ifndef GC_API_H_
#define GC_API_H_

#include "gc-config.h"
#include "gc-assert.h"
#include "gc-inline.h"
#include "gc-ref.h"
#include "gc-edge.h"

#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

// FIXME: prefix with gc_
struct heap;
struct mutator;

enum {
  GC_OPTION_FIXED_HEAP_SIZE,
  GC_OPTION_PARALLELISM
};

struct gc_option {
  int option;
  double value;
};

struct gc_mutator {
  void *user_data;
};

// FIXME: Conflict with bdw-gc GC_API.  Switch prefix?
#ifndef GC_API_
#define GC_API_ static
#endif

GC_API_ int gc_option_from_string(const char *str);
GC_API_ int gc_init(int argc, struct gc_option argv[],
                    struct heap **heap, struct mutator **mutator);

struct gc_mutator_roots;
struct gc_heap_roots;
GC_API_ void gc_mutator_set_roots(struct mutator *mut,
                                  struct gc_mutator_roots *roots);
GC_API_ void gc_heap_set_roots(struct heap *heap, struct gc_heap_roots *roots);

GC_API_ struct mutator* gc_init_for_thread(uintptr_t *stack_base,
                                           struct heap *heap);
GC_API_ void gc_finish_for_thread(struct mutator *mut);
GC_API_ void* gc_call_without_gc(struct mutator *mut, void* (*f)(void*),
                                 void *data) GC_NEVER_INLINE;
GC_API_ void gc_print_stats(struct heap *heap);

enum gc_allocator_kind {
  GC_ALLOCATOR_INLINE_BUMP_POINTER,
  GC_ALLOCATOR_INLINE_FREELIST,
  GC_ALLOCATOR_INLINE_NONE
};

static inline enum gc_allocator_kind gc_allocator_kind(void) GC_ALWAYS_INLINE;

static inline size_t gc_allocator_large_threshold(void) GC_ALWAYS_INLINE;

static inline size_t gc_allocator_small_granule_size(void) GC_ALWAYS_INLINE;

static inline size_t gc_allocator_allocation_pointer_offset(void) GC_ALWAYS_INLINE;
static inline size_t gc_allocator_allocation_limit_offset(void) GC_ALWAYS_INLINE;

static inline size_t gc_allocator_freelist_offset(size_t size) GC_ALWAYS_INLINE;

static inline size_t gc_allocator_alloc_table_alignment(void) GC_ALWAYS_INLINE;
static inline uint8_t gc_allocator_alloc_table_begin_pattern(void) GC_ALWAYS_INLINE;
static inline uint8_t gc_allocator_alloc_table_end_pattern(void) GC_ALWAYS_INLINE;

static inline int gc_allocator_needs_clear(void) GC_ALWAYS_INLINE;

static inline void gc_clear_fresh_allocation(struct gc_ref obj,
                                             size_t size) GC_ALWAYS_INLINE;
static inline void gc_clear_fresh_allocation(struct gc_ref obj,
                                             size_t size) {
  if (!gc_allocator_needs_clear()) return;
  memset(gc_ref_heap_object(obj), 0, size);
}

static inline void gc_update_alloc_table(struct mutator *mut,
                                         struct gc_ref obj,
                                         size_t size) GC_ALWAYS_INLINE;
static inline void gc_update_alloc_table(struct mutator *mut,
                                         struct gc_ref obj,
                                         size_t size) {
  size_t alignment = gc_allocator_alloc_table_alignment();
  if (!alignment) return;

  uintptr_t addr = gc_ref_value(obj);
  uintptr_t base = addr & ~(alignment - 1);
  size_t granule_size = gc_allocator_small_granule_size();
  uintptr_t granule = (addr & (alignment - 1)) / granule_size;
  uint8_t *alloc = (uint8_t*)(base + granule);

  uint8_t begin_pattern = gc_allocator_alloc_table_begin_pattern();
  uint8_t end_pattern = gc_allocator_alloc_table_end_pattern();
  if (end_pattern) {
    size_t granules = size / granule_size;
    if (granules == 1) {
      alloc[0] = begin_pattern | end_pattern;
    } else {
      alloc[0] = begin_pattern;
      if (granules > 2)
        memset(alloc + 1, 0, granules - 2);
      alloc[granules - 1] = end_pattern;
    }
  } else {
    alloc[0] = begin_pattern;
  }
}

GC_API_ void* gc_allocate_small(struct mutator *mut, size_t bytes) GC_NEVER_INLINE;
GC_API_ void* gc_allocate_large(struct mutator *mut, size_t bytes) GC_NEVER_INLINE;

static inline void*
gc_allocate_bump_pointer(struct mutator *mut, size_t size) GC_ALWAYS_INLINE;
static inline void* gc_allocate_bump_pointer(struct mutator *mut, size_t size) {
  GC_ASSERT(size <= gc_allocator_large_threshold());

  size_t granule_size = gc_allocator_small_granule_size();
  size_t hp_offset = gc_allocator_allocation_pointer_offset();
  size_t limit_offset = gc_allocator_allocation_limit_offset();

  uintptr_t base_addr = (uintptr_t)mut;
  uintptr_t *hp_loc = (uintptr_t*)(base_addr + hp_offset);
  uintptr_t *limit_loc = (uintptr_t*)(base_addr + limit_offset);

  size = (size + granule_size - 1) & ~(granule_size - 1);
  uintptr_t hp = *hp_loc;
  uintptr_t limit = *limit_loc;
  uintptr_t new_hp = hp + size;

  if (GC_UNLIKELY (new_hp > limit))
    return gc_allocate_small(mut, size);

  *hp_loc = new_hp;

  gc_clear_fresh_allocation(gc_ref(hp), size);
  gc_update_alloc_table(mut, gc_ref(hp), size);

  return (void*)hp;
}

static inline void* gc_allocate_freelist(struct mutator *mut,
                                         size_t size) GC_ALWAYS_INLINE;
static inline void* gc_allocate_freelist(struct mutator *mut, size_t size) {
  GC_ASSERT(size <= gc_allocator_large_threshold());

  size_t freelist_offset = gc_allocator_freelist_offset(size);
  uintptr_t base_addr = (uintptr_t)mut;
  void **freelist_loc = (void**)(base_addr + freelist_offset);

  void *head = *freelist_loc;
  if (GC_UNLIKELY(!head))
    return gc_allocate_small(mut, size);

  *freelist_loc = *(void**)head;

  gc_clear_fresh_allocation(gc_ref_from_heap_object(head), size);
  gc_update_alloc_table(mut, gc_ref_from_heap_object(head), size);

  return head;
}

static inline void* gc_allocate(struct mutator *mut, size_t bytes) GC_ALWAYS_INLINE;
static inline void* gc_allocate(struct mutator *mut, size_t size) {
  GC_ASSERT(size != 0);
  if (size > gc_allocator_large_threshold())
    return gc_allocate_large(mut, size);

  switch (gc_allocator_kind()) {
  case GC_ALLOCATOR_INLINE_BUMP_POINTER:
    return gc_allocate_bump_pointer(mut, size);
  case GC_ALLOCATOR_INLINE_FREELIST:
    return gc_allocate_freelist(mut, size);
  case GC_ALLOCATOR_INLINE_NONE:
    return gc_allocate_small(mut, size);
  default:
    abort();
  }
}

// FIXME: remove :P
static inline void* gc_allocate_pointerless(struct mutator *mut, size_t bytes);

enum gc_write_barrier_kind {
  GC_WRITE_BARRIER_NONE,
  GC_WRITE_BARRIER_CARD
};

static inline enum gc_write_barrier_kind gc_small_write_barrier_kind(void);
static inline size_t gc_small_write_barrier_card_table_alignment(void);
static inline size_t gc_small_write_barrier_card_size(void);

static inline void gc_small_write_barrier(struct gc_ref obj, struct gc_edge edge,
                                          struct gc_ref new_val) GC_ALWAYS_INLINE;
static inline void gc_small_write_barrier(struct gc_ref obj, struct gc_edge edge,
                                          struct gc_ref new_val) {
  switch (gc_small_write_barrier_kind()) {
  case GC_WRITE_BARRIER_NONE:
    return;
  case GC_WRITE_BARRIER_CARD: {
    size_t card_table_alignment = gc_small_write_barrier_card_table_alignment();
    size_t card_size = gc_small_write_barrier_card_size();
    uintptr_t addr = gc_ref_value(obj);
    uintptr_t base = addr & ~(card_table_alignment - 1);
    uintptr_t card = (addr & (card_table_alignment - 1)) / card_size;
    atomic_store_explicit((uint8_t*)(base + card), 1, memory_order_relaxed);
    return;
  }
  default:
    abort();
  }
}

#endif // GC_API_H_
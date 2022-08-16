#ifndef QUADS_EMBEDDER_H
#define QUADS_EMBEDDER_H

#include "quads-types.h"

#define DEFINE_METHODS(name, Name, NAME) \
  static inline size_t name##_size(Name *obj) GC_ALWAYS_INLINE; \
  static inline void visit_##name##_fields(Name *obj,\
                                           void (*visit)(struct gc_edge edge, void *visit_data), \
                                           void *visit_data) GC_ALWAYS_INLINE;
FOR_EACH_HEAP_OBJECT_KIND(DEFINE_METHODS)
#undef DEFINE_METHODS

static inline size_t quad_size(Quad *obj) {
  return sizeof(Quad);
}

static inline void
visit_quad_fields(Quad *quad,
                  void (*visit)(struct gc_edge edge, void *visit_data),
                  void *visit_data) {
  for (size_t i = 0; i < 4; i++)
    visit(gc_edge(&quad->kids[i]), visit_data);
}

#include "simple-gc-embedder.h"

#endif // QUADS_EMBEDDER_H
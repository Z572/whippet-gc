#ifndef GC_EMBEDDER_API_H
#define GC_EMBEDDER_API_H

#include "gc-edge.h"
#include "gc-forwarding.h"

#ifndef GC_EMBEDDER_API
#define GC_EMBEDDER_API static
#endif

struct gc_mutator_roots;
struct gc_heap_roots;
struct gc_atomic_forward;

GC_EMBEDDER_API inline int gc_has_conservative_roots(void);
GC_EMBEDDER_API inline int gc_has_conservative_intraheap_edges(void);

GC_EMBEDDER_API inline void gc_trace_object(struct gc_ref ref,
                                            void (*trace_edge)(struct gc_edge edge,
                                                               void *trace_data),
                                            void *trace_data,
                                            size_t *size) GC_ALWAYS_INLINE;
GC_EMBEDDER_API inline void gc_trace_conservative_mutator_roots(struct gc_mutator_roots *roots,
                                                                void (*trace_ref)(struct gc_ref edge,
                                                                                  void *trace_data),
                                                                void *trace_data);
GC_EMBEDDER_API inline void gc_trace_precise_mutator_roots(struct gc_mutator_roots *roots,
                                                   void (*trace_edge)(struct gc_edge edge,
                                                                      void *trace_data),
                                                   void *trace_data);
GC_EMBEDDER_API inline void gc_trace_precise_heap_roots(struct gc_heap_roots *roots,
                                                        void (*trace_edge)(struct gc_edge edge,
                                                                           void *trace_data),
                                                        void *trace_data);
GC_EMBEDDER_API inline void gc_trace_conservative_heap_roots(struct gc_heap_roots *roots,
                                                             void (*trace_ref)(struct gc_ref ref,
                                                                               void *trace_data),
                                                             void *trace_data);

GC_EMBEDDER_API inline uintptr_t gc_object_forwarded_nonatomic(struct gc_ref ref);
GC_EMBEDDER_API inline void gc_object_forward_nonatomic(struct gc_ref ref,
                                                        struct gc_ref new_ref);

GC_EMBEDDER_API inline struct gc_atomic_forward gc_atomic_forward_begin(struct gc_ref ref);
GC_EMBEDDER_API inline void gc_atomic_forward_acquire(struct gc_atomic_forward *);
GC_EMBEDDER_API inline int gc_atomic_forward_retry_busy(struct gc_atomic_forward *);
GC_EMBEDDER_API inline void gc_atomic_forward_abort(struct gc_atomic_forward *);
GC_EMBEDDER_API inline void gc_atomic_forward_commit(struct gc_atomic_forward *,
                                                     struct gc_ref new_ref);
GC_EMBEDDER_API inline uintptr_t gc_atomic_forward_address(struct gc_atomic_forward *);


#endif // GC_EMBEDDER_API_H

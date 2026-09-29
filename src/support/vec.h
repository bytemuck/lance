#ifndef LANCE_VEC_H
#define LANCE_VEC_H

#include <stddef.h>

#include "memory.h"

// Generic growable array.
//
//     typedef VEC(TypedDecl) TypedDeclVec;
//     TypedDeclVec decls = {0};
//     VEC_PUSH(decls, decl);
//     for (size_t i = 0; i < decls.count; i++) Use(decls.items[i]);
//     VEC_FREE(decls);
#define VEC(T) struct { T* items; size_t count; size_t capacity; }

#define VEC_RESERVE(vec, needed)                                                          \
    do {                                                                                  \
        if ((needed) > (vec).capacity) {                                                  \
            size_t vecOldCapacity_ = (vec).capacity;                                      \
            size_t vecNewCapacity_ = GROW_CAPACITY(vecOldCapacity_);                      \
            while (vecNewCapacity_ < (needed)) vecNewCapacity_ *= 2;                      \
            (vec).items = GROW_ARRAY(typeof(*(vec).items), (vec).items,                   \
                                     vecOldCapacity_, vecNewCapacity_);                   \
            (vec).capacity = vecNewCapacity_;                                             \
        }                                                                                 \
    } while (0)

#define VEC_PUSH(vec, value)                                                              \
    do {                                                                                  \
        VEC_RESERVE(vec, (vec).count + 1);                                                \
        (vec).items[(vec).count++] = (value);                                             \
    } while (0)

#define VEC_FREE(vec)                                                                     \
    do {                                                                                  \
        if ((vec).items) FREE_ARRAY(typeof(*(vec).items), (vec).items, (vec).capacity);   \
        (vec).items = nullptr;                                                            \
        (vec).count = 0;                                                                  \
        (vec).capacity = 0;                                                               \
    } while (0)

#endif // LANCE_VEC_H

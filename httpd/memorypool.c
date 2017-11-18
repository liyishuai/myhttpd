#include "macros.h"
#include "memorypool.h"
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

/**
 * Align to 2x word size (as GNU libc does).
 */
#define ALIGN_SIZE (2 * sizeof(void*))

/**
 * Round up 'n' to a multiple of ALIGN_SIZE.
 */
#define ROUND_TO_ALIGN(n) ((n+(ALIGN_SIZE-1)) & (~(ALIGN_SIZE-1)))

struct MemoryPool {
    char* memory;
    size_t size;
    size_t pos;
    size_t end;
    httpd_status is_mmap;
};

struct MemoryPool* httpd_pool_create(size_t max) {
    struct MemoryPool* pool;

    pool = malloc(sizeof(struct MemoryPool));
    if (NULL == pool) return NULL;

    pool->memory = mmap(NULL, max, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (MAP_FAILED == pool->memory || NULL == pool->memory) {
        pool->memory = malloc(max);
        if (NULL == pool->memory) {
            free(pool);
            return NULL;
        }
        pool->is_mmap = HTTPD_NO;
    } else {
        pool->is_mmap = HTTPD_YES;
    }
    pool->size = max;
    pool->pos = 0;
    pool->end = max;
    return pool;
}

void httpd_pool_destroy(struct MemoryPool* pool) {
    if (NULL == pool)
        return;
    if (HTTPD_NO == pool->is_mmap)
        free(pool->memory);
    else
        munmap(pool->memory, pool->size);
    free(pool);
}

void* httpd_pool_allocate(struct MemoryPool* pool,
                          size_t size, httpd_status from_end) {
    void* ret;
    size_t asize;

    asize = ROUND_TO_ALIGN(size);
    if (0 == asize && 0 != size)
        return NULL;
    if ((pool->pos + asize > pool->end) || (pool->pos + asize < pool->pos))
        return NULL;
    if (HTTPD_YES == from_end) {
        ret = &pool->memory[pool->end - asize];
        pool->end = pool->end - asize;
    } else {
        ret = &pool->memory[pool->pos];
        pool->pos = pool->pos + asize;
    }
    return ret;
}

void* httpd_pool_reallocate(struct MemoryPool* pool,
                            void* old, size_t old_size, size_t new_size) {
    void* ret;
    size_t asize;

    asize = ROUND_TO_ALIGN(new_size);
    if ( (0 == asize) && (0 != new_size) )
        return NULL;
    if ((pool->end < old_size) || (pool->end < asize))
        return NULL;

    if ((pool->pos >= old_size) &&
        (&pool->memory[pool->pos - old_size] == old)) {
        if (pool->pos + asize - old_size <= pool->end) {
            pool->pos = pool->pos + asize - old_size;
            if (asize < old_size)
                memset(&pool->memory[pool->pos], 0, old_size - asize);
            return old;
        }
        return NULL;
    }
    if (asize <= old_size)
        return old;
    if (pool->pos + asize >= pool->pos &&
        pool->pos + asize <= pool->end) {
        ret = &pool->memory[pool->pos];
        memmove(ret, old, old_size);
        pool->pos = pool->pos + asize;
        return ret;
    }
    return NULL;
}

void* httpd_pool_reset(struct MemoryPool* pool,
                       void* keep, size_t copy_bytes, size_t new_size) {
    if (NULL != keep) {
        if (keep != pool->memory) {
            memmove(pool->memory, keep, copy_bytes);
            keep = pool->memory;
        }
    }
    pool->end = pool->size;
    memset(&pool->memory[copy_bytes], 0, pool->size - copy_bytes);
    if (NULL != keep)
        pool->pos = ROUND_TO_ALIGN(new_size);
    return keep;
}

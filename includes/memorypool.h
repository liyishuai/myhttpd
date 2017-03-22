//
//  memorypool.h
//  myhttpd
//
//  Created by lastland on 16/01/2017.
//  Copyright © 2017 DeepSpec. All rights reserved.
//

#ifndef memorypool_h
#define memorypool_h

#include "internal.h"

struct MemoryPool;

struct MemoryPool* httpd_pool_create(size_t max);

void httpd_pool_destroy(struct MemoryPool* pool);

void* httpd_pool_allocate(struct MemoryPool* pool,
                          size_t size, httpd_status from_end);

void* httpd_pool_reallocate(struct MemoryPool* pool,
                            void* old, size_t old_size, size_t new_size);

void* httpd_pool_reset(struct MemoryPool* pool,
                       void* keep, size_t copy_bytes, size_t new_size);

#endif /* memorypool_h */

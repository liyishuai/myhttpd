//
//  response.c
//  myhttpd
//
//  Created by lastland on 11/02/2017.
//  Copyright © 2017 DeepSpec. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "httpd.h"
#include "response.h"
#include "internal.h"

const char *
HTTPD_get_response_header (struct httpd_response *response,
                           const char *key)
{
    struct httpd_HTTP_header *pos;
    
    if (NULL == key)
        return NULL;
    for (pos = response->first_header; NULL != pos; pos = pos->next)
        if (0 == strcmp (key, pos->header))
            return pos->value;
    return NULL;
}

void
HTTPD_destroy_response (struct httpd_response *response)
{
    struct httpd_HTTP_header *pos;
    
    if (NULL == response)
        return;
    // mutex?
    // response->crfc?
    while (NULL != response->first_header)
    {
        pos = response->first_header;
        response->first_header = pos->next;
        free (pos->header);
        free (pos->value);
        free (pos);
    }
    free (response);
}

struct httpd_response*
HTTPD_create_response_from_data (size_t size,
                                 void *data, int must_free, int must_copy)
{
    struct httpd_response *response;
    void *tmp;
    
    if ((NULL == data) && (size > 0))
        return NULL;
    if (NULL == (response = malloc (sizeof (struct httpd_response))))
        return NULL;
    memset (response, 0, sizeof (struct httpd_response));
    response->fd = -1;
    // mutex?
    if ((must_copy) && (size > 0))
    {
        tmp = malloc(size);
        if (NULL == tmp) {
            // mutex?
            free(response);
            return NULL;
        }
        memcpy (tmp, data, size);
        must_free = HTTPD_YES;
        data = tmp;
    }
    response->crc = NULL;
    response->crfc = must_free ? &free : NULL;
    response->crc_cls = must_free ? data : NULL;
    response->total_size = size;
    response->data = data;
    response->data_size = size;
    return response;
}

struct httpd_response*
HTTPD_create_response_from_buffer (size_t size,
                                   void *buffer,
                                   enum HTTPD_ResponseMemoryMode mode)
{
    int r1, r2;
    r1 = HTTPD_RESPMEM_MUST_FREE == mode;
    r2 = HTTPD_RESPMEM_MUST_COPY == mode;
    return HTTPD_create_response_from_data(size, buffer, r1, r2);
}

struct httpd_response*
HTTPD_create_response_from_callback (uint64_t size,
                                     size_t block_size,
                                     HTTPD_ContentReaderCallback crc,
                                     void *crc_cls,
                                     HTTPD_ContentReaderFreeCallback crfc)
{
    struct httpd_response *response;
    
    if ((NULL == crc) || (0 == block_size))
        return NULL;
    if (NULL == (response = malloc (sizeof (struct httpd_response) + block_size)))
        return NULL;
    memset (response, 0, sizeof (struct httpd_response));
    response->fd = -1;
    response->data = (void *) &response[1];
    response->data_buffer_size = block_size;
    response->crc = crc;
    response->crfc = crfc;
    response->crc_cls = crc_cls;
    response->total_size = size;
    return response;
}

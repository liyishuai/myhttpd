//
//  main.c
//  myhttpd
//
//  Created by lastland on 03/01/2017.
//  Copyright © 2017 DeepSpec. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "httpd.h"

#define PAGE "<html><head><title>DeepSpec Web Server</title></head><body>Hello DeepSpec!</body></html>"

static int ahc_echo (void *cls,
                     struct httpd_connection *connection,
                     const char *url,
                     const char *method,
                     const char *version,
                     const char *upload_data, size_t *upload_data_size, void **ptr) {
    static int aptr;
    const char *me = cls;
    struct httpd_response *response;
    int ret;
    
    if (0 != strcmp (method, "GET"))
        return HTTPD_NO;              /* unexpected method */
    if (&aptr != *ptr)
    {
        /* do never respond on first call */
        *ptr = &aptr;
        return HTTPD_YES;
    }
    *ptr = NULL;                  /* reset when done */
    response = HTTPD_create_response_from_buffer (strlen (me),
                                                  (void *) me,
                                                  HTTPD_RESPMEM_PERSISTENT);
    ret = HTTPD_queue_response (connection, HTTP_OK, response);
    //HTTPD_destroy_response (response);
    return ret;
}

int main (int argc, char *const *argv) {
    struct httpd_daemon *d;
    
    if (argc != 2)
    {
        printf ("%s PORT\n", argv[0]);
        return 1;
    }
    d = create_daemon(atoi(argv[1]), &ahc_echo, PAGE);
    if (d == NULL)
        return 1;
    (void) getc (stdin);
    stop_daemon (d);
    return 0;
}

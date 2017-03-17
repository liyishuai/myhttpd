//
//  main.c
//  myhttpd
//
//  Created by lastland on 03/01/2017.
//  Copyright © 2017 DeepSpec. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "httpd.h"

#define PAGE "<html><head><title>404 not found</title></head><body>404 not found</body></html>"

#define DIR "/Users/lastland/Downloads/web/html/"


static ssize_t
file_reader (void *cls,
             uint64_t pos,
             char *buf,
             size_t max)
{
    FILE *file = cls;
    
    (void)  fseek (file, pos, SEEK_SET);
    return fread (buf, 1, max, file);
}


static void
free_callback (void *cls)
{
    FILE *file = cls;
    fclose (file);
}


static int
ahc_echo (void *cls,
          struct httpd_connection *connection,
          const char *url,
          const char *method,
          const char *version,
          const char *upload_data,
          size_t *upload_data_size, void **ptr)
{
    static int aptr;
    struct httpd_response *response;
    int ret;
    FILE *file;
    int fd;
    struct stat buf;
    char file_name[255];
    
    if ( (0 != strcmp (method, HTTPD_HTTP_METHOD_GET)) &&
        (0 != strcmp (method, HTTPD_HTTP_METHOD_HEAD)) )
        return HTTPD_NO;              /* unexpected method */
    if (&aptr != *ptr)
    {
        /* do never respond on first call */
        *ptr = &aptr;
        return HTTPD_YES;
    }
    *ptr = NULL;                  /* reset when done */
    
    sprintf(file_name, DIR "/%s", &url[1]);
    file = fopen (file_name, "rb");
    while (NULL != file)
    {
        fd = fileno (file);
        if (-1 == fd)
        {
            (void) fclose (file);
            return HTTPD_NO; /* internal error */
        }
        if ( (0 != fstat (fd, &buf)) ||
            (!S_ISREG (buf.st_mode)) )
        {
            /* not a regular file, refuse to serve */
            fclose (file);
            file = NULL;
        }
        if (S_ISDIR(buf.st_mode)) {
            /* is a dir, open `index.html` */
            sprintf(file_name, DIR "%s/index.html", &url[1]);
            file = fopen (file_name, "rb");
            continue;
        }
        break;
    }
    if (NULL == file)
    {
        response = HTTPD_create_response_from_buffer (strlen (PAGE),
                                                      (void *) PAGE,
                                                      HTTPD_RESPMEM_PERSISTENT);
        ret = HTTPD_queue_response (connection, HTTP_NOT_FOUND, response);
        //HTTPD_destroy_response (response);
    }
    else
    {
        response = HTTPD_create_response_from_callback (buf.st_size, 32 * 1024,     /* 32k page size */
                                                        &file_reader,
                                                        file,
                                                        &free_callback);
        if (NULL == response)
        {
            fclose (file);
            return HTTPD_NO;
        }
        ret = HTTPD_queue_response (connection, HTTP_OK, response);
        //HTTPD_destroy_response (response);
    }
    return ret;
}


int
main (int argc, char *const *argv)
{
    struct httpd_daemon *d;
    
    if (argc != 2)
    {
        printf ("%s PORT\n", argv[0]);
        return 1;
    }
    d = create_daemon (atoi (argv[1]), &ahc_echo, PAGE);
    if (d == NULL)
        return 1;
    (void) getc (stdin);
    stop_daemon (d);
    return 0;
}

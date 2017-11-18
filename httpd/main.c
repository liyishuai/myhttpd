#include "macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "httpd.h"

#define PAGE "<html><head><title>404 not found</title></head><body>404 not found</body></html>"

#define DIR "web/"


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

static void open_file_and_get_stat (const char *file_name,
                                    FILE **cls,
                                    struct stat* st)
{
    FILE *file;
    struct stat buf;
    int fd, ret;

    file = fopen(file_name, "rb");
    if (NULL == file)
    {
        *cls = NULL;
        return;
    }
    else
    {
        fd = fileno (file);
        if (-1 == fd)
        {
            fclose(file);
            *cls = NULL;
            return;
        }
        ret = fstat (fd, &buf);
        if (0 != ret)
        {
            fclose(file);
            *cls = NULL;
            return;
        }
        *cls = file;
        *st = buf;
    }
}

static httpd_status send_not_found (struct httpd_connection *conn)
{
    httpd_status ret;
    struct httpd_response *response;

    response = HTTPD_create_response_from_buffer (strlen (PAGE),
                                                  (void *) PAGE,
                                                  HTTPD_RESPMEM_PERSISTENT);
    ret = HTTPD_queue_response (conn, HTTP_NOT_FOUND, response);
    return ret;
}

static httpd_status respond_file (FILE* file,
                                  struct httpd_connection *conn,
                                  off_t size)
{
    httpd_status ret;
    struct httpd_response *response;

    response = HTTPD_create_response_from_callback (size,
                                                    32 * 1024,     /* 32k page size */
                                                    &file_reader,
                                                    file,
                                                    &free_callback);
    if (NULL == response)
    {
        fclose (file);
        return HTTPD_NO;
    }
    ret = HTTPD_queue_response (conn, HTTP_OK, response);
    return ret;
}

static httpd_status
ahc_echo (void *cls,
          struct httpd_connection *connection,
          const char *url,
          const char *method,
          const char *version,
          const char *upload_data,
          size_t *upload_data_size, void **ptr)
{
    static int aptr;
    int ret;
    FILE *file;
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

    strcpy(file_name, DIR);
    strcat(file_name, &url[1]);
    open_file_and_get_stat (file_name, &file, &buf);
    if (NULL == file)
    {
        ret = send_not_found(connection);
        return ret;
    }
    else
    {
        if (S_ISDIR (buf.st_mode))
        {
            fclose (file);
            strcat(file_name, "/index.html");
            open_file_and_get_stat (file_name, &file, &buf);
            if (NULL == file)
            {
                ret = send_not_found (connection);
                return ret;
            }
            if (S_ISREG (buf.st_mode))
            {
                ret = respond_file (file, connection, buf.st_size);
                return ret;
            }
            fclose (file);
            ret = send_not_found (connection);
            return ret;
        }
        else if (S_ISREG (buf.st_mode))
        {
            ret = respond_file (file, connection, buf.st_size);
            return ret;
        }
        fclose (file);
        ret = send_not_found (connection);
        return ret;
    }
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
    pause();
    stop_daemon (d);
    return 0;
}

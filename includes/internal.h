//
//  internal.h
//  myhttpd
//
//  Created by lastland on 03/01/2017.
//  Copyright © 2017 DeepSpec. All rights reserved.
//

#ifndef internal_h
#define internal_h

#define HTTPD_YES 0
#define HTTPD_NO -1

#include "httpd.h"

typedef pthread_t httpd_thread_handle;


enum httpd_connectionEventLoopInfo
{
    HTTPD_EVENT_LOOP_INFO_READ = 0,
    HTTPD_EVENT_LOOP_INFO_WRITE = 1,
    HTTPD_EVENT_LOOP_INFO_BLOCK = 2,
    HTTPD_EVENT_LOOP_INFO_CLEANUP = 3
};


struct httpd_connection {
    struct sockaddr* addr;
    socklen_t addr_len;
    struct httpd_daemon* daemon;
    httpd_socket socket;
    
    struct httpd_response *response;
    
    int (*read_handler)(struct httpd_connection *conn);
    int (*write_handler)(struct httpd_connection *conn);
    int (*idle_handler)(struct httpd_connection *conn);
    
    struct httpd_connection* prev;
    struct httpd_connection* next;
    enum httpd_connectionEventLoopInfo event_loop_info;
    
    char* read_buffer;
    size_t read_buffer_size;
    size_t read_buffer_offset;
};


struct httpd_response {
    
};

struct httpd_daemon {
    httpd_socket socket;
    httpd_thread_handle pid;
    httpd_status shutdown;
    
    unsigned int connections;
    unsigned int connection_limit;
    struct httpd_connection* connections_head;
    struct httpd_connection* connections_tail;
    int at_limit;
};


#endif /* internal_h */

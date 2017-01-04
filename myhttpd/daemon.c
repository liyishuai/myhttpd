//
//  daemon.c
//  myhttpd
//
//  Created by lastland on 03/01/2017.
//  Copyright © 2017 DeepSpec. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "httpd.h"
#include "internal.h"

#ifdef DEBUG
void httpd_log(char* c) {
    printf("%s\n", c);
}
#endif

static httpd_status make_noninheritable(httpd_socket socket) {
    int flags, r;
    flags = fcntl(socket, F_GETFD);
    if (-1 == flags) {
        return HTTPD_NO;
    }
    
    if (flags != (flags | FD_CLOEXEC)) {
        r = fcntl(socket, F_SETFD, flags | FD_CLOEXEC);
    }
    if (r != 0) {
        return HTTPD_NO;
    }
    
    return HTTPD_YES;
}

static httpd_status make_nonblocking(httpd_socket socket) {
    int flags, r;
    flags = fcntl(socket, F_GETFL);
    if (-1 == flags) {
        return HTTPD_NO;
    }
    
    if (flags != (flags | O_NONBLOCK)) {
        r = fcntl(socket, F_SETFL, flags | O_NONBLOCK);
    }
    if (r != 0) {
        return HTTPD_NO;
    }
    
    return HTTPD_YES;
}

httpd_socket create_listen_socket(struct httpd_daemon* daemon) {
    httpd_socket fd;
    fd = socket(AF_INET, SOCK_STREAM, 0);

    if (INVALID_SOCKET == fd) {
        return INVALID_SOCKET;
    }
    
    make_noninheritable(fd);
    
    return fd;
}

struct httpd_daemon* create_daemon(uint16_t port) {

    struct httpd_daemon* daemon;
    httpd_socket socket_fd;
    httpd_sockaddr socket_addr;
    struct sockaddr* servaddr;
    socklen_t addr_len;
    int r;
    
    /* initialize daemon */
    daemon = malloc(sizeof(struct httpd_daemon));
    memset(daemon, 0, sizeof(struct httpd_daemon));
    daemon->socket = INVALID_SOCKET;

    /* create a socket */
    socket_fd = create_listen_socket(daemon);
    daemon->socket = socket_fd;
    
    /* bind the socket to the given port */
    memset(&socket_addr, 0, sizeof(httpd_sockaddr));
    addr_len = sizeof(httpd_sockaddr);
    socket_addr.sin_family = AF_INET;
    socket_addr.sin_port = port;
    socket_addr.sin_len = addr_len;
    servaddr = (struct sockaddr*) &socket_addr;
    
    r = bind(socket_fd, servaddr, addr_len);
    if (-1 == r) {
#ifdef DEBUG
        httpd_log("Failed to bind.");
#endif
        goto free_and_fail;
    }
    
    /* start listening */
    r = listen(socket_fd, SOMAXCONN);
    if (-1 == r) {
#ifdef DEBUG
        httpd_log("Failed to listen.");
#endif
        goto free_and_fail;
    }
    make_nonblocking(socket_fd);
    
    return daemon;
    
free_and_fail:
    free(daemon);
    return NULL;
}

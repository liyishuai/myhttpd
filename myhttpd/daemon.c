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
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include "httpd.h"
#include "internal.h"
#include "connection.h"
#include "ipc.h"


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

static void make_nonblocking_noninheritable(httpd_socket socket) {
    make_nonblocking(socket);
    make_noninheritable(socket);
}

static ssize_t recv_param_adapter(struct httpd_connection* conn,
                                  void* other, size_t i) {
    ssize_t ret;
    
    if (INVALID_SOCKET == conn->socket ||
        HTTPD_CONNECTION_CLOSED == conn->state) {
        errno = ENOTCONN;
        return -1;
    }
    if (i > SSIZE_MAX)
        i = SSIZE_MAX;
    
    ret = recv(conn->socket, other, i, 0);
    
    return ret;
}

static ssize_t send_param_adapter(struct httpd_connection* conn,
                                  const void* other, size_t i) {
    ssize_t ret;
    
    if (INVALID_SOCKET == conn->socket ||
        HTTPD_CONNECTION_CLOSED == conn->state) {
        errno = ENOTCONN;
        return -1;
    }
    if (i > SSIZE_MAX)
        i = SSIZE_MAX;
    
    ret = send(conn->socket, other, i, 0);
    
    /* Handle broken kernel / libc, returning -1 but not setting errno;
     kill connection as that should be safe; reported on mailinglist here:
     http://lists.gnu.org/archive/html/libmicrohttpd/2014-10/msg00023.html */
    if ( (0 > ret) && (0 == errno))
        errno = ECONNRESET;
    return ret;
}

typedef void* (*ThreadStartRoutine) (void* cls);

static httpd_status create_thread(httpd_thread_handle* thread,
                                  struct httpd_daemon* daemon,
                                  ThreadStartRoutine start_routine,
                                  void* arg) {
    int r;
    r = pthread_create(thread, NULL, start_routine, arg);
    return r;
}

static httpd_status add_to_fd_set(httpd_socket fd,
                                  fd_set* set,
                                  httpd_socket* max_fd,
                                  unsigned int fd_setsize) {
    if (NULL == set)
        return HTTPD_NO;
    if (fd >= (httpd_socket)fd_setsize)
        return HTTPD_NO;
    FD_SET(fd, set);
    if (NULL != max_fd && INVALID_SOCKET != fd) {
        if (fd > *max_fd || INVALID_SOCKET == *max_fd) {
            *max_fd = fd;
        }
    }
    return HTTPD_YES;
}

static httpd_status httpd_get_fdset2(struct httpd_daemon* daemon,
                                     fd_set* read_fd_set,
                                     fd_set* write_fd_set,
                                     fd_set* except_fd_set,
                                     httpd_socket* max_fd,
                                     unsigned int fd_setsize) {
    httpd_status r, result;
    struct httpd_connection* pos;
    result = HTTPD_YES;
    
    if (NULL == daemon ||
        NULL == read_fd_set ||
        NULL == write_fd_set ||
        HTTPD_YES == daemon->shutdown) {
        return HTTPD_NO;
    }
    
    if (INVALID_SOCKET != daemon->socket) {
        r = add_to_fd_set(daemon->socket, read_fd_set, max_fd, fd_setsize);
        if (HTTPD_YES != r)
            result = HTTPD_NO;
    }
    
    for (pos = daemon->connections_head; NULL != pos; pos = pos->next) {
        switch (pos->event_loop_info) {
            case HTTPD_EVENT_LOOP_INFO_READ:
                r = add_to_fd_set(pos->socket, read_fd_set, max_fd, fd_setsize);
                if (HTTPD_YES != r)
                    result = r;
                break;
            case HTTPD_EVENT_LOOP_INFO_WRITE:
                r = add_to_fd_set(pos->socket, write_fd_set, max_fd, fd_setsize);
                if (HTTPD_YES != r)
                    result = r;
                if (pos->read_buffer_size > pos->read_buffer_offset) {
                    r = add_to_fd_set(pos->socket, read_fd_set, max_fd, fd_setsize);
                    if (HTTPD_YES != r)
                        result = r;
                }
                break;
            case HTTPD_EVENT_LOOP_INFO_BLOCK:
                if (pos->read_buffer_size > pos->read_buffer_offset) {
                    r = add_to_fd_set(pos->socket, read_fd_set, max_fd, fd_setsize);
                    if (HTTPD_YES != r)
                        result = r;
                }
                break;
            case HTTPD_EVENT_LOOP_INFO_CLEANUP:
                break;
        }
    }
    
    return result;
}

static httpd_status internal_add_connection(struct httpd_daemon* daemon,
                                            httpd_socket client_socket,
                                            const struct sockaddr* addr,
                                            socklen_t addrlen,
                                            int external_add) {
    struct httpd_connection* connection;
    static int on = 1;
    
    if (client_socket >= FD_SETSIZE) {
        close(client_socket);
        errno = EINVAL;
        return HTTPD_NO;
    }
    
    /*
    if (daemon->connections == daemon->connection_limit) {
        close(client_socket);
        errno = ENFILE;
        return HTTPD_NO;
    }
     */
    
    setsockopt(client_socket, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
    
    connection = malloc(sizeof(struct httpd_connection));
    if (NULL == connection) {
        int eno = errno;
        close(client_socket);
        errno = eno;
        return HTTPD_NO;
    }
    memset(connection, 0, sizeof(struct httpd_connection));
    
    connection->pool = httpd_pool_create(daemon->pool_size);
    if (NULL == connection->pool) {
        close(client_socket);
        errno = ENOMEM;
        return HTTPD_NO;
    }
    
    // TODO: connection timeout
    
    connection->addr = malloc(addrlen);
    if (NULL == connection->addr) {
        int eno = errno;
        httpd_pool_destroy(connection->pool);
        free(connection);
        errno = eno;
        return HTTPD_NO;
    }
    
    memcpy(connection->addr, addr, addrlen);
    connection->addr_len = addrlen;
    connection->socket = client_socket;
    connection->daemon = daemon;
    
    connection->read_handler = &httpd_connection_handle_read;
    connection->write_handler = &httpd_connection_handle_write;
    connection->idle_handler = &httpd_connection_handle_idle;
    connection->recv_cls = &recv_param_adapter;
    connection->send_cls = &send_param_adapter;

    
    connection->next = daemon->connections_head;
    connection->prev = NULL;
    if (NULL == daemon->connections_tail)
        daemon->connections_tail = connection;
    else
        daemon->connections_head->prev = connection;
    daemon->connections_head = connection;
    
    // TODO: external_add is yes
    
    daemon->connections++;
    return HTTPD_YES;
}

static httpd_status accept_connection(struct httpd_daemon* daemon) {
    struct sockaddr_in sock_addr;
    struct sockaddr* addr;
    socklen_t addrlen;
    httpd_socket s;
    httpd_socket fd;
    
    addr = (struct sockaddr*)&sock_addr;
    addrlen = sizeof(sock_addr);
    memset(addr, 0, addrlen);
    fd = daemon->socket;
    if (INVALID_SOCKET == fd)
        return HTTPD_NO;

    s = accept(fd, addr, &addrlen);
    if (INVALID_SOCKET == s || addrlen <= 0) {
        const int err = errno;
        if (EINVAL == err && INVALID_SOCKET == daemon->socket) {
            return HTTPD_NO;
        }
        if (INVALID_SOCKET != s) {
            close(s);
        }
        if (EMFILE == err ||
            ENFILE == err ||
            ENOMEM == err ||
            ENOBUFS == err) {
            if (0 != daemon->connections) {
                daemon->at_limit = 1;
            }
        }
        return HTTPD_NO;
    }
    else
        printf("s = %d\n", s);
    make_nonblocking_noninheritable(s);
    internal_add_connection(daemon, s, addr, addrlen, HTTPD_NO);
    
    return HTTPD_YES;
}

static httpd_status call_handlers(struct httpd_connection* conn,
                                  int read_ready,
                                  int write_ready,
                                  httpd_status force_close) {
    httpd_status had_response_before_idle, r;
    
    if (read_ready)
        conn->read_handler(conn);
    if (write_ready)
        conn->write_handler(conn);
    if (NULL != conn->response)
        had_response_before_idle = HTTPD_YES;
    // TODO: force close
    r = conn->idle_handler(conn);
    return r;
}

static httpd_status httpd_cleanup_connections(struct httpd_daemon* daemon) {
    return HTTPD_YES;
}

static httpd_status run_from_select(struct httpd_daemon* daemon,
                                    const fd_set* rs,
                                    const fd_set* ws,
                                    const fd_set* es) {
    httpd_socket ds;
    struct httpd_connection *pos;
    struct httpd_connection *next;
    
    // TODO: drain a pipe?
    
    ds = daemon->socket;
    if (INVALID_SOCKET != ds && FD_ISSET(ds, rs)) {
        accept_connection(daemon);
    }
    next = daemon->connections_head;
    pos = next;
    while (NULL != pos) {
        next = pos->next;
        ds = pos->socket;
        if (INVALID_SOCKET == ds)
            continue;
        call_handlers(pos, FD_ISSET(ds, rs), FD_ISSET(ds, ws), HTTPD_NO);
        pos = next;
    }
    httpd_cleanup_connections(daemon);
    return HTTPD_YES;
}

static httpd_status httpd_select(struct httpd_daemon* daemon,
                                 httpd_status mayblock) {
    int num_ready;
    fd_set rs;
    fd_set ws;
    fd_set es;
    httpd_socket maxsock;
    struct timeval timeout;
    struct timeval* tv;
    int r;
    httpd_status err_state;
    
    if (HTTPD_YES == daemon->shutdown) {
        return HTTPD_NO;
    }
    
    maxsock = INVALID_SOCKET;
    FD_ZERO(&rs);
    FD_ZERO(&ws);
    FD_ZERO(&es);
    
    r = httpd_get_fdset2(daemon, &rs, &ws, &es, &maxsock, FD_SETSIZE);
    if (HTTPD_NO == r) {
        err_state = HTTPD_YES;
    }
    
    if (INVALID_SOCKET != daemon->socket) {
        if (daemon->connections == daemon->connection_limit &&
            daemon->at_limit) {
            FD_CLR(daemon->socket, &rs);
        }
    }
    
    if (HTTPD_YES == err_state)
        mayblock = HTTPD_NO;

    if (HTTPD_NO == mayblock) {
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
    } else {
        // temporary
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
    }
    tv = &timeout;
    num_ready = select(maxsock + 1, &rs, &ws, &es, tv);
    
    if (HTTPD_YES == daemon->shutdown)
        return HTTPD_NO;
    if (num_ready < 0) {
        if (HTTPD_YES == err_state)
            return HTTPD_NO;
        else
            return HTTPD_YES;
    }
    
    r = run_from_select(daemon, &rs, &ws, &es);
    if (HTTPD_YES == r) {
        if (HTTPD_YES == err_state)
            return HTTPD_NO;
        else
            return HTTPD_YES;
    }
    return HTTPD_NO;
}

static void* select_thread(void* cls) {
    struct httpd_daemon* daemon = cls;
    while (HTTPD_YES != daemon->shutdown) {
        httpd_select(daemon, HTTPD_YES);
        httpd_cleanup_connections(daemon);
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

const char *mem_name = "ipcm";
const char *sem_name = "ipcs";

struct httpd_daemon* create_daemon(uint16_t port,
                                   HTTPD_AccessHandlerCallback dh,
                                   void* dh_cls) {

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
    daemon->shutdown = HTTPD_NO;
    daemon->pool_size = HTTPD_POOL_SIZE_DEFAULT;
    daemon->pool_increment = HTTPD_BUF_INC_SIZE;
    daemon->default_handler = dh;
    daemon->default_handler_cls = dh_cls;

#ifdef ipc_h
    /* initialize ipc */
    if (ipc_init(mem_name, sem_name) == -1)
    {
#ifdef DEBUG
        httpd_log("Failed to initialize IPC.");
#endif /* DEBUG */
        goto free_and_fail;
    }
#endif /* ipc_h */

    /* create a socket */
    socket_fd = create_listen_socket(daemon);
    daemon->socket = socket_fd;
    
    /* bind the socket to the given port */
    memset(&socket_addr, 0, sizeof(httpd_sockaddr));
    addr_len = sizeof(httpd_sockaddr);
    socket_addr.sin_family = AF_INET;
    socket_addr.sin_port = htons(port);
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
    
    r = create_thread(&daemon->pid, daemon, select_thread, daemon);
    
    return daemon;
    
free_and_fail:
    free(daemon);
#ifdef ipc_h
    ipc_close();
#endif
    return NULL;
}

void stop_daemon(struct httpd_daemon* daemon) {
    //int fd;
    
    if (NULL == daemon)
        return;
    
    daemon->shutdown = HTTPD_YES;
    //fd = daemon->socket;
    daemon->socket = INVALID_SOCKET;
    // TODO: worker pool?
    pthread_join(daemon->pid, NULL);
    // TODO: close all connections
    free(daemon);
#ifdef ipc_h
    ipc_close();
#endif
}

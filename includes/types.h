//
//  types.h
//  myhttpd
//
//  Created by Yishuai Li on 01/20/2017.
//  Copyright © 2017 DeepSpec. All rights reserved.
//

#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>

#ifndef types_h
#define types_h

typedef uint8_t     sa_family_t;
typedef uint32_t    socklen_t;

struct sockaddr;

typedef enum {
    ACCEPT,
    BIND,
    CLOSE,
    FCNTL,
    LISTEN,
    SELECT,
    SOCKET
#ifdef DEBUG
    , TEST
#endif
} opcode;

typedef struct {
    int socket;
    struct sockaddr address;
    socklen_t address_len;
} accept_args_t;

typedef struct {
    int socket;
    struct sockaddr address;
    socklen_t address_len;
} bind_args_t;

typedef struct {
    int fildes;
} close_args_t;

typedef struct {
    int fildes;
    int cmd;
    int arg;
} fcntl_args_t;

typedef struct {
    int socket;
    int backlog;
} listen_args_t;

typedef struct {
    int nfds;
    fd_set readfds;
    fd_set writefds;
    fd_set errorfds;
    struct timeval timeout;
} select_args_t;

typedef struct {
    int domain;
    int type;
    int protocol;
} socket_args_t;

#ifdef DEBUG
typedef struct {
    int a;
    int b;
} test_args_t;
#endif

typedef int accept_ret_t;
typedef int bind_ret_t;
typedef int close_ret_t;
typedef int fcntl_ret_t;
typedef int listen_ret_t;
typedef int select_ret_t;
typedef int socket_ret_t;
#ifdef DEBUG
typedef int test_ret_t;
#endif

typedef union {
    accept_args_t       accept_args;
    bind_args_t         bind_args;
    close_args_t        close_args;
    fcntl_args_t        fcntl_args;
    listen_args_t       listen_args;
    select_args_t       select_args;
    socket_args_t       socket_args;
#ifdef DEBUG
    test_args_t         test_args;
#endif
} args_t;

typedef union {
    accept_ret_t        accept_ret;
    bind_ret_t          bind_ret;
    close_ret_t         close_ret;
    fcntl_ret_t         fcntl_ret;
    listen_ret_t        listen_ret;
    select_ret_t        select_ret;
    socket_ret_t        socket_ret;
#ifdef DEBUG
    test_ret_t          test_ret;
#endif
} ret_t;

typedef struct {
    opcode op;
    args_t args;
    ret_t  ret;
} msg_t;

#define MSG_SIZE sizeof(msg_t)

#endif /* types_h */

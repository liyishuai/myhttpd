//
//  ipc.c
//  myhttpd
//
//  Created by Yishuai Li on 01/19/2017.
//  Copyright © 2017 DeepSpec. All rights reserved.
//

#include "ipc.h"

typedef enum {
    ACCEPT,
    BIND,
    LISTEN,
    SETSOCKOPT,
    SOCKET
} opcode;

typedef struct {
    int socket;
    struct sockaddr *restrict address;
    socklen_t *restrict address_len;
} accept_args_t;

typedef struct {
    int socket;
    const struct sockaddr *address;
    socklen_t address_len;
} bind_args_t;

typedef struct {
    int socket;
    int backlog;
} listen_args_t;

typedef struct {
    int socket;
    int level;
    int option_name;
    const void *option_value;
    socklen_t option_len;
} setsockopt_args_t;

typedef struct {
    int domain;
    int type;
    int protocol;
} socket_args_t;

typedef int accept_ret_t;
typedef int bind_ret_t;
typedef int listen_ret_t;
typedef int setsockopt_ret_t;
typedef int socket_ret_t;

typedef union {
    accept_args_t       accept_args;
    bind_args_t         bind_args;
    listen_args_t       listen_args;
    setsockopt_args_t   setsockopt_args;
    socket_args_t       socket_args;
} args_t;

typedef union {
    accept_ret_t        accept_ret;
    bind_ret_t          bind_ret;
    listen_ret_t        listen_ret;
    setsockopt_ret_t    setsockopt_ret;
    socket_ret_t        socket_ret;
} ret_t;

typedef struct {
    size_t seq;
    opcode op;
    args_t args;
} req_t;

typedef struct {
    size_t seq;
    ret_t  ret;
} ack_t;

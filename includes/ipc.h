//
//  ipc.h
//  myhttpd
//
//  Created by Yishuai Li on 01/19/2017.
//  Copyright © 2017 DeepSpec. All rights reserved.
//

#include "types.h"

#ifndef ipc_h
#define ipc_h

int accept(int socket,
           struct sockaddr * __restrict address,
           socklen_t * __restrict address_len);
int bind(int socket,
         const struct sockaddr *address,
         socklen_t address_len);
int close1(int fildes);
int fcntl3(int fildes,
           int cmd,
           int arg);
int fcntl2(int fildes,
           int cmd);
int listen(int socket,
           int backlog);
ssize_t recv(int socket,
             void *buffer,
             size_t length,
             int flags);
int select(int nfds,
           fd_set *restrict readfds,
           fd_set *restrict writefds,
           fd_set *restrict errorfds,
           struct timeval *restrict timeout);
ssize_t send(int socket,
             const void *buffer,
             size_t length,
             int flags);
int setsockopt(int socket,
               int level,
               int option_name,
               const void *option_value,
               socklen_t option_len);
int socket(int domain,
           int type,
           int protocol);
#ifdef DEBUG
int test(int a, int b);
#endif

int ipc_init(const char *mem_name, const char *sem_name);
void ipc_close();
#ifdef DEBUG
void ipc_test();
#endif

#endif /* ipc_h */

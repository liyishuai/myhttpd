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
int listen(int socket,
           int backlog);
int select(int nfds,
           fd_set *restrict readfds,
           fd_set *restrict writefds,
           fd_set *restrict errorfds,
           struct timeval *restrict timeout);
int socket(int domain,
           int type,
           int protocol);
#ifdef DEBUG
int test(int a, int b);
#endif

void* ipc_init(const char *name);
void ipc_close();
#ifdef DEBUG
void ipc_test();
#endif

#endif /* ipc_h */

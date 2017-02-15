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

int accept(int, struct sockaddr * __restrict, socklen_t * __restrict);
int bind(int, const struct sockaddr *, socklen_t);
int listen(int, int);
int setsockopt(int, int, int, const void *, socklen_t);
int socket(int, int, int);
#ifdef DEBUG
int test(int, int);
#endif

void* ipc_init(const char *);
void ipc_close();
#ifdef DEBUG
void ipc_test();
#endif

#endif /* ipc_h */

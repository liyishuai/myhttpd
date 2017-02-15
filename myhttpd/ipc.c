//
//  ipc.c
//  myhttpd
//
//  Created by Yishuai Li on 01/19/2017.
//  Copyright © 2017 DeepSpec. All rights reserved.
//

#include "ipc.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

int  ipc_fd;
msg_t *ipc_mem;
pid_t daemon_pid;
sigset_t mask;

void ipc_close()
{
    munmap(ipc_mem, MSG_SIZE);
    close(ipc_fd);
}

void* ipc_init(const char *name)
{
    printf("client pid: %d\n", getpid());
    printf("daemon pid: ");
    scanf("%d", &daemon_pid);

    // sigemptyset(&mask);
    // sigaddset(&mask, SIGUSR1);
    sigfillset(&mask);

    ipc_fd = shm_open(name, O_RDWR);
    if (ipc_fd == -1)
    {
        fputs(strerror(errno), stderr);
        goto error;
    }
    ipc_mem = mmap(0, MSG_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, ipc_fd, 0);
    if (ipc_mem == MAP_FAILED)
    {
        fputs(strerror(errno), stderr);
        goto error;
    }
    return ipc_mem;

error:
    ipc_close();
    return NULL;
}

ret_t call(opcode op, args_t args)
{
    ipc_mem->op = ACCEPT;
    ipc_mem->args = args;
    kill(daemon_pid, SIGUSR1);
    int sig;
    int errnum = sigwait(&mask, &sig);
    if (errnum != 0)
        fputs(strerror(errnum), stderr);
    return ipc_mem->ret;
}

int accept(int socket,
           struct sockaddr * __restrict address,
           socklen_t * __restrict address_len)
{
    args_t args;
    args.accept_args.socket = socket;
    args.accept_args.address = address;
    args.accept_args.address_len = address_len;
    return call(ACCEPT, args).accept_ret;
}

int bind(int socket,
         const struct sockaddr *address,
         socklen_t address_len)
{
    args_t args;
    args.bind_args.socket = socket;
    args.bind_args.address = address;
    args.bind_args.address_len = address_len;
    return call(BIND, args).bind_ret;
}

int listen(int socket,
           int backlog)
{
    args_t args;
    args.listen_args.socket = socket;
    args.listen_args.backlog = backlog;
    return call(LISTEN, args).listen_ret;
}

int setsocketopt(int socket,
                 int level,
                 int option_name,
                 const void *option_value,
                 socklen_t option_len)
{
    args_t args;
    args.setsockopt_args.socket = socket;
    args.setsockopt_args.level = level;
    args.setsockopt_args.option_name = option_name;
    args.setsockopt_args.option_value = option_value;
    args.setsockopt_args.option_len = option_len;
    return call(SETSOCKOPT, args).setsockopt_ret;
}

int socket(int domain,
           int type,
           int protocol)
{
    args_t args;
    args.socket_args.domain = domain;
    args.socket_args.type = type;
    args.socket_args.protocol = protocol;
    return call(SOCKET, args).socket_ret;
}

#ifdef DEBUG
int test(int a, int b)
{
    args_t args;
    args.test_args.a = a;
    args.test_args.b = b;
    return call(TEST, args).test_ret;
}
#endif

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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>

int  ipc_fd;
msg_t *ipc_mem;
pid_t daemon_pid;
sigset_t mask;
bool ready;
struct timespec rqtp;

void ipc_close()
{
    munmap(ipc_mem, MSG_SIZE);
    close(ipc_fd);
}

void respond()
{
    ready = true;
}

void* ipc_init(const char *name)
{
    printf("client pid: %d\n", getpid());
    printf("daemon pid: ");
    scanf("%d", &daemon_pid);

    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    struct sigaction action;
    action.sa_handler = respond;
    action.sa_mask = mask;
    action.sa_flags = 0;
    sigaction(SIGUSR1, &action, NULL);

    ipc_fd = shm_open(name, O_RDWR, S_IRWXU);
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
    ipc_mem->op = op;
    ipc_mem->args = args;
    ready = false;
    kill(daemon_pid, SIGUSR1);
    for (rqtp.tv_sec = 0, rqtp.tv_nsec = 1; !ready;
         rqtp.tv_sec += rqtp.tv_sec + rqtp.tv_nsec / 500000000,
         rqtp.tv_nsec = rqtp.tv_nsec * 2 % 1000000000)
    {
        int errnum = nanosleep(&rqtp, NULL);
        if (errnum != 0)
            fputs(strerror(errnum), stderr);
    }
    return ipc_mem->ret;
}

int accept(int socket,
           struct sockaddr * __restrict address,
           socklen_t * __restrict address_len)
{
    args_t args;
    args.accept_args.socket = socket;
    args.accept_args.address.sa_len = address->sa_len;
    args.accept_args.address.sa_family = address->sa_family;
    memcpy(args.accept_args.address.sa_data,
           address->sa_data,
           sizeof address->sa_data);
    args.accept_args.address_len = *address_len;
    accept_ret_t ret = call(ACCEPT, args).accept_ret;
    address->sa_len = args.accept_args.address.sa_len;
    address->sa_family = args.accept_args.address.sa_family;
    memcpy(address->sa_data,
           args.accept_args.address.sa_data,
           sizeof address->sa_data);
    *address_len = args.accept_args.address_len;
    return ret;
}

int bind(int socket,
         const struct sockaddr *address,
         socklen_t address_len)
{
    args_t args;
    args.bind_args.socket = socket;
    args.bind_args.address.sa_len = address->sa_len;
    args.bind_args.address.sa_family = address->sa_family;
    memcpy(args.bind_args.address.sa_data,
           address->sa_data,
           sizeof address->sa_data);
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

int select(int nfds,
           fd_set *restrict readfds,
           fd_set *restrict writefds,
           fd_set *restrict errorfds,
           struct timeval *restrict timeout)
{
    args_t args;
    args.select_args.nfds = nfds;
    memcpy(args.select_args.readfds.fds_bits,
           readfds->fds_bits,
           sizeof readfds->fds_bits);
    memcpy(args.select_args.writefds.fds_bits,
           writefds->fds_bits,
           sizeof writefds->fds_bits);
    memcpy(args.select_args.errorfds.fds_bits,
           errorfds->fds_bits,
           sizeof errorfds->fds_bits);
    args.select_args.timeout.tv_sec = timeout->tv_sec;
    args.select_args.timeout.tv_usec = timeout->tv_usec;
    select_ret_t ret = call(SELECT, args).select_ret;
    memcpy(readfds->fds_bits,
           args.select_args.readfds.fds_bits,
           sizeof readfds->fds_bits);
    memcpy(writefds->fds_bits,
           args.select_args.writefds.fds_bits,
           sizeof writefds->fds_bits);
    memcpy(writefds->fds_bits,
           args.select_args.writefds.fds_bits,
           sizeof writefds->fds_bits);
    timeout->tv_sec = args.select_args.timeout.tv_sec;
    timeout->tv_usec = args.select_args.timeout.tv_usec;
    return ret;
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

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
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>

int  ipc_fd;
msg_t *ipc_mem;
sem_t *ipc_sem;
pid_t daemon_pid;
struct timespec rqtp;

void ipc_close()
{
    munmap(ipc_mem, MSG_SIZE);
    close(ipc_fd);
    sem_close(ipc_sem);
}

int ipc_init(const char *mem_name, const char *sem_name)
{
    printf("server pid: %d\n", getpid());
    printf("daemon pid: ");
    scanf("%d", &daemon_pid);

    ipc_fd = shm_open(mem_name, O_RDWR, S_IRWXU);
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
    ipc_sem = sem_open(sem_name, 0);

    return 0;

error:
    ipc_close();
    return -1;
}

ret_t call(opcode op, args_t *args)
{
    ipc_mem->op = op;
    ipc_mem->args = *args;
    kill(daemon_pid, SIGUSR1);
    sem_wait(ipc_sem);
    *args = ipc_mem->args;
    return ipc_mem->ret;
}

int accept(int socket,
           struct sockaddr * __restrict address,
           socklen_t * __restrict address_len)
{
    args_t args;
    args.accept_args.socket = socket;
    memcpy(&args.accept_args.address, address, *address_len);
    args.accept_args.address_len = *address_len;
    accept_ret_t ret = call(ACCEPT, &args).accept_ret;
    *address_len = args.accept_args.address_len;
    memcpy(address,
           &args.accept_args.address,
           *address_len);
    return ret;
}

int bind(int socket,
         const struct sockaddr *address,
         socklen_t address_len)
{
    args_t args;
    args.bind_args.socket = socket;
    memcpy(&args.bind_args.address,
           address,
           address_len);
    args.bind_args.address_len = address_len;
    return call(BIND, &args).bind_ret;
}

int close1(int fildes)
{
    args_t args;
    args.close_args.fildes = fildes;
    return call(CLOSE, &args).close_ret;
}

int fcntl3(int fildes,
           int cmd,
           int arg)
{
    args_t args;
    args.fcntl_args.fildes = fildes;
    args.fcntl_args.cmd = cmd;
    args.fcntl_args.arg = arg;
    return call(FCNTL, &args).fcntl_ret;
}

int fcntl2(int fildes,
           int cmd)
{
    return fcntl3(fildes, cmd, 0);
}

int listen(int socket,
           int backlog)
{
    args_t args;
    args.listen_args.socket = socket;
    args.listen_args.backlog = backlog;
    return call(LISTEN, &args).listen_ret;
}

ssize_t recv(int socket,
             void *buffer,
             size_t length,
             int flags)
{
    args_t args;
    args.recv_args.socket = socket;
    args.recv_args.length = length;
    args.recv_args.flags = flags;
    recv_ret_t ret = call(RECV, &args).recv_ret;
    memcpy(buffer, args.recv_args.buffer, length);
    return ret;
}

int select(int nfds,
           fd_set *__restrict readfds,
           fd_set *__restrict writefds,
           fd_set *__restrict errorfds,
           struct timeval *__restrict timeout)
{
    args_t args;
    args.select_args.nfds = nfds;
    args.select_args.readfds  = *readfds;
    args.select_args.writefds = *writefds;
    args.select_args.errorfds = *errorfds;
    args.select_args.timeout.tv_sec = timeout->tv_sec;
    args.select_args.timeout.tv_usec = timeout->tv_usec;
    select_ret_t ret = call(SELECT, &args).select_ret;
    *readfds  = args.select_args.readfds;
    *writefds = args.select_args.writefds;
    *errorfds = args.select_args.errorfds;
    timeout->tv_sec = args.select_args.timeout.tv_sec;
    timeout->tv_usec = args.select_args.timeout.tv_usec;
    return ret;
}

ssize_t send(int socket,
             const void *buffer,
             size_t length,
             int flags)
{
    args_t args;
    args.send_args.socket = socket;
    args.send_args.length = length;
    args.send_args.flags = flags;
    memcpy(args.send_args.buffer, buffer, length);
    return call(SEND, &args).send_ret;
}

int setsockopt(int socket,
               int level,
               int option_name,
               const void *option_value,
               socklen_t option_len)
{
    args_t args;
    args.setsockopt_args.socket = socket;
    args.setsockopt_args.level = level;
    args.setsockopt_args.option_name = option_name;
    args.setsockopt_args.option_len = option_len;
    memcpy(args.setsockopt_args.option_value, option_value, option_len);
    return call(SETSOCKOPT, &args).setsockopt_ret;
}

int socket(int domain,
           int type,
           int protocol)
{
    args_t args;
    args.socket_args.domain = domain;
    args.socket_args.type = type;
    args.socket_args.protocol = protocol;
    return call(SOCKET, &args).socket_ret;
}

#ifdef DEBUG
int test(int a, int b)
{
    args_t args;
    args.test_args.a = a;
    args.test_args.b = b;
    return call(TEST, &args).test_ret;
}
#endif

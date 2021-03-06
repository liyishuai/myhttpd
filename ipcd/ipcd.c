//
//  ipcd.c
//  ipcd
//
//  Created by Yishuai Li on 02/09/2017.
//  Copyright © 2017 DeepSpec. All rights reserved.
//

#include "ipcd.h"
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

int  ipcd_fd;
msg_t *ipcd_mem;
sem_t *ipcd_sem;
pid_t client_pid;

void respond()
{
    switch (ipcd_mem->op) {
        case ACCEPT:
#ifdef DEBUG
            fprintf(stderr, "ACCEPT %d %d\n",
                   ipcd_mem->args.accept_args.socket,
                   ipcd_mem->args.accept_args.address_len);
#endif
            ipcd_mem->ret.accept_ret =
            accept(ipcd_mem->args.accept_args.socket,
                   &ipcd_mem->args.accept_args.address,
                   &ipcd_mem->args.accept_args.address_len);
            break;
        case BIND:
#ifdef DEBUG
            fprintf(stderr, "BIND %d %d\n",
                   ipcd_mem->args.bind_args.socket,
                   ipcd_mem->args.bind_args.address_len);
#endif
            ipcd_mem->ret.bind_ret =
            bind(ipcd_mem->args.bind_args.socket,
                 &ipcd_mem->args.bind_args.address,
                 ipcd_mem->args.bind_args.address_len);
            break;
        case CLOSE:
#ifdef DEBUG
            fprintf(stderr, "CLOSE %d\n",
                   ipcd_mem->args.close_args.fildes);
#endif
            ipcd_mem->ret.close_ret =
            close(ipcd_mem->args.close_args.fildes);
            break;
        case FCNTL:
#ifdef DEBUG
            fprintf(stderr, "FCNTL %d %d %d\n",
                   ipcd_mem->args.fcntl_args.fildes,
                   ipcd_mem->args.fcntl_args.cmd,
                   ipcd_mem->args.fcntl_args.arg);
#endif
            ipcd_mem->ret.fcntl_ret =
            fcntl(ipcd_mem->args.fcntl_args.fildes,
                  ipcd_mem->args.fcntl_args.cmd,
                  ipcd_mem->args.fcntl_args.arg);
            break;
        case LISTEN:
#ifdef DEBUG
            fprintf(stderr, "LISTEN %d %d\n",
                   ipcd_mem->args.listen_args.socket,
                   ipcd_mem->args.listen_args.backlog);
#endif
            ipcd_mem->ret.listen_ret =
            listen(ipcd_mem->args.listen_args.socket,
                   ipcd_mem->args.listen_args.backlog);
            break;
        case RECV:
#ifdef DEBUG
            fprintf(stderr, "RECV %d %lu %d\n",
                   ipcd_mem->args.recv_args.socket,
                   ipcd_mem->args.recv_args.length,
                   ipcd_mem->args.recv_args.flags);
#endif
            ipcd_mem->ret.recv_ret =
            recv(ipcd_mem->args.recv_args.socket,
                 ipcd_mem->args.recv_args.buffer,
                 ipcd_mem->args.recv_args.length,
                 ipcd_mem->args.recv_args.flags);
#ifdef DEBUG
            fputs(ipcd_mem->args.recv_args.buffer, stderr);
#endif
            break;
        case SELECT:
#ifdef DEBUG
            fprintf(stderr, "SELECT %d %ld %d\n",
                   ipcd_mem->args.select_args.nfds,
                   ipcd_mem->args.select_args.timeout.tv_sec,
                   ipcd_mem->args.select_args.timeout.tv_usec);
#endif
            ipcd_mem->ret.select_ret =
            select(ipcd_mem->args.select_args.nfds,
                   &ipcd_mem->args.select_args.readfds,
                   &ipcd_mem->args.select_args.writefds,
                   &ipcd_mem->args.select_args.errorfds,
                   &ipcd_mem->args.select_args.timeout);
            break;
        case SEND:
#ifdef DEBUG
            fprintf(stderr, "SEND %d %s %lu %d\n",
                   ipcd_mem->args.send_args.socket,
                   ipcd_mem->args.send_args.buffer,
                   ipcd_mem->args.send_args.length,
                   ipcd_mem->args.send_args.flags);
#endif
            ipcd_mem->ret.send_ret =
            send(ipcd_mem->args.send_args.socket,
                 ipcd_mem->args.send_args.buffer,
                 ipcd_mem->args.send_args.length,
                 ipcd_mem->args.send_args.flags);
            break;
        case SETSOCKOPT:
#ifdef DEBUG
            fprintf(stderr, "SETSOCKOPT %d %d %d %d %u\n",
                   ipcd_mem->args.setsockopt_args.socket,
                   ipcd_mem->args.setsockopt_args.level,
                   ipcd_mem->args.setsockopt_args.option_name,
                   *(int *)ipcd_mem->args.setsockopt_args.option_value,
                   ipcd_mem->args.setsockopt_args.option_len);
#endif
            ipcd_mem->ret.setsockopt_ret =
            setsockopt(ipcd_mem->args.setsockopt_args.socket,
                       ipcd_mem->args.setsockopt_args.level,
                       ipcd_mem->args.setsockopt_args.option_name,
                       ipcd_mem->args.setsockopt_args.option_value,
                       ipcd_mem->args.setsockopt_args.option_len);
            break;
        case SOCKET:
#ifdef DEBUG
            fprintf(stderr, "SOCKET %d %d %d\n",
                   ipcd_mem->args.socket_args.domain,
                   ipcd_mem->args.socket_args.type,
                   ipcd_mem->args.socket_args.protocol);
#endif
            ipcd_mem->ret.socket_ret =
            socket(ipcd_mem->args.socket_args.domain,
                   ipcd_mem->args.socket_args.type,
                   ipcd_mem->args.socket_args.protocol);
            break;
#ifdef DEBUG
        case TEST:
            fprintf(stderr, "TEST: %d + %d\n",
                   ipcd_mem->args.test_args.a,
                   ipcd_mem->args.test_args.b);
            ipcd_mem->ret.test_ret =
            ipcd_mem->args.test_args.a + ipcd_mem->args.test_args.b;
            break;
#endif
        default:
            return;
    }
#ifdef DEBUG
    fprintf(stderr, "return %d\n", ipcd_mem->ret.accept_ret);
#endif
    sem_post(ipcd_sem);
}

void ipcd_close(const char *mem_name, const char *sem_name)
{
    munmap(ipcd_mem, MSG_SIZE);
    close(ipcd_fd);
    shm_unlink(mem_name);
    sem_close(ipcd_sem);
    sem_unlink(sem_name);
}

int ipcd_init(const char *mem_name, const char *sem_name)
{
    ipcd_fd = shm_open(mem_name, O_RDWR | O_CREAT | O_EXCL, S_IRWXU);
    if (ipcd_fd == -1)
    {
        fputs(strerror(errno), stderr);
        goto error;
    }
    if (ftruncate(ipcd_fd, MSG_SIZE))
    {
        fputs(strerror(errno), stderr);
        goto error;
    }
    ipcd_mem = mmap(0, MSG_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, ipcd_fd, 0);
    if (ipcd_mem == MAP_FAILED)
    {
        fputs(strerror(errno), stderr);
        goto error;
    }

    ipcd_sem = sem_open(sem_name, O_CREAT | O_EXCL, S_IRWXU, 0);
    if (ipcd_sem == SEM_FAILED)
    {
        fputs(strerror(errno), stderr);
        goto error;
    }

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    struct sigaction action;
    action.sa_handler = respond;
    action.sa_mask = mask;
    action.sa_flags = 0;
    sigaction(SIGUSR1, &action, NULL);

    fprintf(stderr, "server pid: ");
    scanf("%d", &client_pid);
    fprintf(stderr, "%d\ndaemon pid: %d\n", client_pid, getpid());
    return 0;

error:
    ipcd_close(mem_name, sem_name);
    return -1;
}

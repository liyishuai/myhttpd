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
            printf("ACCEPT %d %d %d %s %d\n",
                   ipcd_mem->args.accept_args.socket,
                   ipcd_mem->args.accept_args.address.sa_len,
                   ipcd_mem->args.accept_args.address.sa_family,
                   ipcd_mem->args.accept_args.address.sa_data,
                   ipcd_mem->args.accept_args.address_len);
            ipcd_mem->ret.accept_ret =
            accept(ipcd_mem->args.accept_args.socket,
                   &ipcd_mem->args.accept_args.address,
                   &ipcd_mem->args.accept_args.address_len);
            break;
        case BIND:
            printf("BIND %d %d %d %s %d\n",
                   ipcd_mem->args.bind_args.socket,
                   ipcd_mem->args.bind_args.address.sa_len,
                   ipcd_mem->args.bind_args.address.sa_family,
                   ipcd_mem->args.bind_args.address.sa_data,
                   ipcd_mem->args.bind_args.address_len);
            ipcd_mem->ret.bind_ret =
            bind(ipcd_mem->args.bind_args.socket,
                 &ipcd_mem->args.bind_args.address,
                 ipcd_mem->args.bind_args.address_len);
            break;
        case LISTEN:
            printf("LISTEN %d %d\n",
                   ipcd_mem->args.listen_args.socket,
                   ipcd_mem->args.listen_args.backlog);
            ipcd_mem->ret.listen_ret =
            listen(ipcd_mem->args.listen_args.socket,
                   ipcd_mem->args.listen_args.backlog);
            break;
        case SELECT:
            printf("SELECT %d %ld %d\n",
                   ipcd_mem->args.select_args.nfds,
                   ipcd_mem->args.select_args.timeout.tv_sec,
                   ipcd_mem->args.select_args.timeout.tv_usec);
            ipcd_mem->ret.select_ret =
            select(ipcd_mem->args.select_args.nfds,
                   &ipcd_mem->args.select_args.readfds,
                   &ipcd_mem->args.select_args.writefds,
                   &ipcd_mem->args.select_args.errorfds,
                   &ipcd_mem->args.select_args.timeout);
            break;
        case SOCKET:
            printf("SOCKET %d %d %d\n",
                   ipcd_mem->args.socket_args.domain,
                   ipcd_mem->args.socket_args.type,
                   ipcd_mem->args.socket_args.protocol);
            ipcd_mem->ret.socket_ret =
            socket(ipcd_mem->args.socket_args.domain,
                   ipcd_mem->args.socket_args.type,
                   ipcd_mem->args.socket_args.protocol);
            break;
#ifdef DEBUG
        case TEST:
            printf("TEST: %d + %d\n",
                   ipcd_mem->args.test_args.a,
                   ipcd_mem->args.test_args.b);
            ipcd_mem->ret.test_ret =
            ipcd_mem->args.test_args.a + ipcd_mem->args.test_args.b;
            break;
#endif
        default:
            return;
    }
    printf("return %d\n", ipcd_mem->ret.accept_ret);
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

    printf("client pid: ");
    scanf("%d", &client_pid);
    printf("daemon pid: %d\n", getpid());
    return 0;

error:
    ipcd_close(mem_name, sem_name);
    return -1;
}

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
#include <signal.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

int  ipcd_fd;
msg_t *ipcd_mem;
pid_t client_pid;

void respond()
{
    printf("respond %d\n", ipcd_mem->op);
    switch (ipcd_mem->op) {
        case ACCEPT:
            ipcd_mem->ret.accept_ret =
            accept(ipcd_mem->args.accept_args.socket,
                   ipcd_mem->args.accept_args.address,
                   ipcd_mem->args.accept_args.address_len);
            break;
        case BIND:
            printf("BIND\t%d\t%p\t%d\n",
                   ipcd_mem->args.bind_args.socket,
                   ipcd_mem->args.bind_args.address,
                   ipcd_mem->args.bind_args.address_len);
            ipcd_mem->ret.bind_ret =
            bind(ipcd_mem->args.bind_args.socket,
                 ipcd_mem->args.bind_args.address,
                 ipcd_mem->args.bind_args.address_len);
            break;
        case LISTEN:
            ipcd_mem->ret.listen_ret =
            listen(ipcd_mem->args.listen_args.socket,
                   ipcd_mem->args.listen_args.backlog);
            break;
        case SETSOCKOPT:
            ipcd_mem->ret.setsockopt_ret =
            setsockopt(ipcd_mem->args.setsockopt_args.socket,
                       ipcd_mem->args.setsockopt_args.level,
                       ipcd_mem->args.setsockopt_args.option_name,
                       ipcd_mem->args.setsockopt_args.option_value,
                       ipcd_mem->args.setsockopt_args.option_len);
            break;
        case SOCKET:
            printf("SOCKET\t%d\t%d\t%d\n",
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
    kill(client_pid, SIGUSR1);
}

void ipcd_close(const char *name)
{
    munmap(ipcd_mem, MSG_SIZE);
    close(ipcd_fd);
    shm_unlink(name);
}

void* ipcd_init(const char *name)
{
    ipcd_fd = shm_open(name, O_RDWR | O_CREAT, S_IRWXU);
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

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigfillset(&mask);
    struct sigaction action;
    action.sa_handler = respond;
    action.sa_mask = mask;
    action.sa_flags = 0;
    sigaction(SIGUSR1, &action, NULL);

    printf("client pid: ");
    scanf("%d", &client_pid);
    printf("daemon pid: %d\n", getpid());
    return ipcd_mem;

error:
    ipcd_close(name);
    return NULL;
}

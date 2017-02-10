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
void *ipcd_mem;
pid_t client_pid;

void respond()
{
    msg_t msg;
    memcpy(&msg, ipcd_mem, MSG_SIZE);
    switch (msg.op) {
        case ACCEPT:
            msg.ret.accept_ret =
            accept(msg.args.accept_args.socket,
                   msg.args.accept_args.address,
                   msg.args.accept_args.address_len);
            break;
        case BIND:
            msg.ret.bind_ret =
            bind(msg.args.bind_args.socket,
                 msg.args.bind_args.address,
                 msg.args.bind_args.address_len);
            break;
        case LISTEN:
            msg.ret.listen_ret =
            listen(msg.args.listen_args.socket,
                   msg.args.listen_args.backlog);
            break;
        case SETSOCKOPT:
            msg.ret.setsockopt_ret =
            setsockopt(msg.args.setsockopt_args.socket,
                       msg.args.setsockopt_args.level,
                       msg.args.setsockopt_args.option_name,
                       msg.args.setsockopt_args.option_value,
                       msg.args.setsockopt_args.option_len);
            break;
        case SOCKET:
            msg.ret.socket_ret =
            socket(msg.args.socket_args.domain,
                   msg.args.socket_args.protocol,
                   msg.args.socket_args.type);
            break;
        default:
            return;
    }
    memcpy(ipcd_mem, &msg, MSG_SIZE);
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
    ipcd_fd = shm_open(name, O_RDWR | O_CREAT);
    if (ipcd_fd == -1)
    {
        puts(strerror(errno));
        goto error;
    }
    if (ftruncate(ipcd_fd, MSG_SIZE))
    {
        puts(strerror(errno));
        goto error;
    }
    ipcd_mem = mmap(0, MSG_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, ipcd_fd, 0);
    if (ipcd_mem == MAP_FAILED)
    {
        puts(strerror(errno));
        goto error;
    }

    sigset_t mask;
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

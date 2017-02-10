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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

int  ipc_fd;
void *ipc_mem;
pid_t daemon_pid;

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

    ipc_fd = shm_open(name, O_RDWR);
    if (ipc_fd == -1)
    {
        puts(strerror(errno));
        goto error;
    }
    ipc_mem = mmap(0, MSG_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, ipc_fd, 0);
    if (ipc_mem == MAP_FAILED)
    {
        puts(strerror(errno));
        goto error;
    }
    return ipc_mem;

error:
    ipc_close();
    return NULL;
}

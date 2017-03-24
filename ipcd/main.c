//
//  main.c
//  ipcd
//
//  Created by Yishuai Li on 02/09/2017.
//  Copyright © 2017 DeepSpec. All rights reserved.
//

#include "ipcd.h"
#include <ctype.h>
#include <stdio.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>

bool terminated;

void terminate()
{
    terminated = true;
}

int main(int argc, const char * argv[]) {
    const char *mem_name = "ipcm";
    const char *sem_name = "ipcs";
    ipcd_init(mem_name, sem_name);

    terminated = false;
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    struct sigaction action;
    action.sa_handler = terminate;
    action.sa_mask = mask;
    action.sa_flags = 0;
    sigaction(SIGINT, &action, NULL);
    while (!terminated)
        pause();

    ipcd_close(mem_name, sem_name);
    
    return 0;
}

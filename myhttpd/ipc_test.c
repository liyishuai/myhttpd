//
//  ipc_test.c
//  myhttpd
//
//  Created by Yishuai Li on 02/10/2017.
//  Copyright © 2017 DeepSpec. All rights reserved.
//

#ifdef DEBUG

#include "ipc.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void ipc_test()
{
    const char *mem_name = "ipcm";
    const char *sem_name = "ipcs";
    if (ipc_init(mem_name, sem_name) != 0)
        goto error;

    srand((unsigned)time(NULL));
    int a = rand();
    int b = rand();
    int c = test(a, b);
    if (c == a + b)
        fprintf(stderr, "IPC test succeed: %d + %d = %d.\n", a, b, c);
    else
        fprintf(stderr, "IPC test failed: %d + %d != %d.\n", a, b, c);

error:
    ipc_close();
}

#endif

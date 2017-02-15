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
    const char *name = "ipcd";
    if (!ipc_init(name))
    {
        fputs(strerror(errno), stderr);
        goto error;
    }

    srand((unsigned)time(NULL));
    int a = rand();
    int b = rand();
    int c = test(a, b);
    if (c == a + b)
        printf("IPC test succeed: %d + %d = %d.\n", a, b, c);
    else
        printf("IPC test failed: %d + %d != %d.\n", a, b, c);

error:
    ipc_close();
    puts("IPC test failed.");
}

#endif

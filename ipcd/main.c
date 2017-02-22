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

int main(int argc, const char * argv[]) {
    const char *mem_name = "ipcm";
    const char *sem_name = "ipcs";
    if (argc > 2)
    {
        mem_name = argv[1];
        sem_name = argv[2];
    }

    ipcd_init(mem_name, sem_name);
    while (tolower(getchar()) != 'q');
    ipcd_close(mem_name, sem_name);
    
    return 0;
}

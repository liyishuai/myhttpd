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
    const char *name = "ipcd";
    if (argc > 1)
        name = argv[1];

    ipcd_init(name);
    while (tolower(getchar()) != 'q');
    ipcd_close(name);
    
    return 0;
}

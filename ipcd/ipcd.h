//
//  ipcd.h
//  ipcd
//
//  Created by Yishuai Li on 02/09/2017.
//  Copyright © 2017 DeepSpec. All rights reserved.
//

#ifndef ipcd_h
#define ipcd_h

#include "types.h"

int ipcd_init(const char *mem_name, const char *sem_name);
void ipcd_close(const char *mem_name, const char *sem_name);

#endif /* ipcd_h */

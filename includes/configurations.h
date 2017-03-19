//
//  configurations.h
//  myhttpd
//
//  Created by lastland on 03/01/2017.
//  Copyright © 2017 DeepSpec. All rights reserved.
//

#ifndef configurations_h
#define configurations_h

#define DEBUG 1

#ifdef __linux__
#define HAVE_SOCKADDR_IN_SIN_LEN 0
#else
#define HAVE_SOCKADDR_IN_SIN_LEN 1
#endif

#endif /* configurations_h */

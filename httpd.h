//
//  httpd.h
//  myhttpd
//
//  Created by lastland on 03/01/2017.
//  Copyright © 2017 DeepSpec. All rights reserved.
//

#ifndef httpd_h
#define httpd_h

#define INVALID_SOCKET -1

#include <netinet/ip.h>

struct httpd_connection;

struct httpd_daemon;

typedef int httpd_socket;

typedef int httpd_status;

typedef struct sockaddr_in httpd_sockaddr;

struct httpd_daemon* create_daemon(uint16_t);

#endif /* httpd_h */

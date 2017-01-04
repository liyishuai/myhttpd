//
//  internal.h
//  myhttpd
//
//  Created by lastland on 03/01/2017.
//  Copyright © 2017 DeepSpec. All rights reserved.
//

#ifndef internal_h
#define internal_h

#define HTTPD_YES 0
#define HTTPD_NO -1

struct httpd_connection {
    
};

struct httpd_daemon {
    httpd_socket socket;
    struct httpd_connection* head;
    struct httpd_connection* tail;
};

#endif /* internal_h */

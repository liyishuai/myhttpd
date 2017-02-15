//
//  connection.h
//  myhttpd
//
//  Created by lastland on 04/01/2017.
//  Copyright © 2017 DeepSpec. All rights reserved.
//

#ifndef connection_h
#define connection_h

#include "httpd.h"

httpd_status httpd_connection_handle_read(struct httpd_connection* conn);
httpd_status httpd_connection_handle_write(struct httpd_connection* conn);
httpd_status httpd_connection_handle_idle(struct httpd_connection* conn);

#endif /* connection_h */

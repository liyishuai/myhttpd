//
//  string.h
//  myhttpd
//
//  Created by lastland on 01/02/2017.
//  Copyright © 2017 DeepSpec. All rights reserved.
//

#ifndef httpd_string_h
#define httpd_string_h

#include <stdio.h>

int
HTTPD_str_equal_caseless_ (const char * const str1,
                           const char * const str2);

size_t
HTTPD_str_to_uint64_ (const char * str,
                      uint64_t * out_val);

size_t
HTTPD_strx_to_sizet_n_ (const char * str,
                      size_t maxlen,
                      size_t * out_val);

#endif /* httpd_string_h */

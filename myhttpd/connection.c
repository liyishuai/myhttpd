//
//  connection.c
//  myhttpd
//
//  Created by lastland on 04/01/2017.
//  Copyright © 2017 DeepSpec. All rights reserved.
//

#include <errno.h>
#include <stdlib.h>
#include "httpd.h"
#include "internal.h"
#include "connection.h"

#define HTTP_100_CONTINUE "HTTP/1.1 100 Continue\r\n\r\n"

const size_t BUFFER_INITIAL_SIZE = 1024;
const size_t BUFFER_INC_SIZE = 1024;


static int try_grow_read_buffer(struct httpd_connection* conn) {
    void* buf;
    size_t new_size;
    
    if (0 == conn->read_buffer_size)
        new_size = BUFFER_INITIAL_SIZE;
    else
        new_size = conn->read_buffer_size + BUFFER_INC_SIZE;
    
}

static char* get_next_header_line(struct httpd_connection* conn,
                                  size_t* line_len) {
    char* rbuf;
    size_t pos;
    
    if (0 == conn->read_buffer_offset)
        return NULL;
    pos = 0;
    rbuf = conn->read_buffer;
    while ((pos < conn->read_buffer_offset - 1) &&
           ('\r' != rbuf[pos]) && ('\n' != rbuf[pos]))
        pos++;
    
}

static void close_connection(struct httpd_connection* conn) {
    struct httpd_daemon* daemon;
    
    daemon = conn->daemon;
    conn->state = HTTPD_CONNECTION_CLOSED;
    conn->event_loop_info = HTTPD_EVENT_LOOP_INFO_CLEANUP;
    // TODO: notify daemon
}

static httpd_status do_read(struct httpd_connection* conn) {
    ssize_t bytes_read;
    
    if (conn->read_buffer_offset == conn->read_buffer_size)
        return HTTPD_NO;
    bytes_read = conn->recv_cls(conn,
                                &conn->read_buffer[conn->read_buffer_offset],
                                conn->read_buffer_size - conn->read_buffer_offset);
    
    if (bytes_read < 0) {
        const int err = errno;
        if (EINTR == err || EAGAIN == err || EWOULDBLOCK == err)
            return HTTPD_NO;
        if (ECONNRESET == err) {
            close_connection(conn);
        }
        close_connection(conn);
        return HTTPD_YES;
    }
    if (0 == bytes_read) {
        conn->read_closed = 1;
        close_connection(conn);
        return HTTPD_YES;
    }
    conn->read_buffer_offset += bytes_read;
    return HTTPD_YES;
}

static httpd_status do_write(struct httpd_connection* conn) {
    ssize_t ret;
    size_t max;
    
    max = conn->write_buffer_append_offset - conn->write_buffer_send_offset;
    ret = conn->send_cls(conn, &conn->write_buffer[conn->write_buffer_send_offset], max);
    
    if (ret < 0) {
        const int err = errno;
        if (EINTR == err || EAGAIN == err || EWOULDBLOCK == err)
            return HTTPD_NO;
        close_connection(conn);
        return HTTPD_YES;
    }
    if (0 != max)
        conn->write_buffer_send_offset += ret;
    return HTTPD_YES;
}

static int check_write_done(struct httpd_connection* conn,
                            enum httpd_connection_state next_state) {
    if (conn->write_buffer_send_offset != conn->write_buffer_append_offset)
        return HTTPD_NO;
    conn->write_buffer_append_offset = 0;
    conn->write_buffer_send_offset = 0;
    conn->state = next_state;
    // TODO: pool reallocate
    conn->write_buffer = NULL;
    conn->write_buffer_size = 0;
    return HTTPD_YES;
}

httpd_status httpd_connection_handle_read(struct httpd_connection* conn) {
    // TODO: grow connection pool
    httpd_status r;
    r = do_read(conn);
    if (HTTPD_NO == r)
        return HTTPD_YES;
    while (1) {
        switch (conn->state) {
            case HTTPD_CONNECTION_INIT:
            case HTTPD_CONNECTION_URL_RECEIVED:
            case HTTPD_CONNECTION_HEADER_PART_RECEIVED:
            case HTTPD_CONNECTION_HEADERS_RECEIVED:
            case HTTPD_CONNECTION_HEADERS_PROCESSED:
            case HTTPD_CONNECTION_CONTINUE_SENDING:
            case HTTPD_CONNECTION_CONTINUE_SENT:
            case HTTPD_CONNECTION_BODY_RECEIVED:
            case HTTPD_CONNECTION_FOOTERS_RECEIVED:
                if (1 == conn->read_closed) {
                    close_connection(conn);
                    continue;
                }
                break;
            case HTTPD_CONNECTION_CLOSED:
                return HTTPD_YES;
            default:
                // TODO: shrink buffer
                break;
        }
        break;
    }
    return HTTPD_YES;
}

httpd_status httpd_connection_handle_write(struct httpd_connection* conn) {
    struct httpd_response* response;
    ssize_t ret;
    
    while (1) {
        switch (conn->state) {
            case HTTPD_CONNECTION_INIT:
            case HTTPD_CONNECTION_URL_RECEIVED:
            case HTTPD_CONNECTION_HEADER_PART_RECEIVED:
            case HTTPD_CONNECTION_HEADERS_RECEIVED:
                //abort();
                break;
            case HTTPD_CONNECTION_HEADERS_PROCESSED:
                break;
            case HTTPD_CONNECTION_CONTINUE_SENDING:
                // TODO: continue writing
                break;
            case HTTPD_CONNECTION_CONTINUE_SENT:
            case HTTPD_CONNECTION_BODY_RECEIVED:
            case HTTPD_CONNECTION_FOOTER_PART_RECEIVED:
            case HTTPD_CONNECTION_FOOTERS_RECEIVED:
                //abort();
                break;
            case HTTPD_CONNECTION_HEADERS_SENDING:
                do_write(conn);
                if (HTTPD_CONNECTION_HEADERS_SENDING != conn->state)
                    break;
                check_write_done(conn, HTTPD_CONNECTION_HEADERS_SENT);
                break;
            case HTTPD_CONNECTION_HEADERS_SENT:
                //abort();
                break;
            case HTTPD_CONNECTION_NORMAL_BODY_READY:
                response = conn->response;
                if (conn->response_write_position <
                    conn->response->total_size) {
                    int err;
                    uint64_t data_write_offset;
                    // TODO: response crc
                    data_write_offset = conn->response_write_position -
                        conn->response->data_start;
                    if (data_write_offset > (uint64_t)SIZE_MAX)
                        return HTTPD_NO;
                    ret = conn->send_cls(conn,
                                         &response->data[(size_t)data_write_offset],
                                         response->data_size - (size_t)data_write_offset);
                    err = errno;
                    conn->response_write_position += ret;
                }
                if (conn->response_write_position == conn->response->total_size)
                    conn->state = HTTPD_CONNECTION_FOOTERS_SENT;
                break;
            case HTTPD_CONNECTION_NORMAL_BODY_UNREADY:
                break;
            case HTTPD_CONNECTION_CHUNKED_BODY_READY:
                do_write(conn);
                if (HTTPD_CONNECTION_CHUNKED_BODY_READY != conn->state)
                    break;
                if (conn->response->total_size == conn->response_write_position)
                    check_write_done(conn, HTTPD_CONNECTION_BODY_SENT);
                else
                    check_write_done(conn, HTTPD_CONNECTION_CHUNKED_BODY_UNREADY);
                break;
            case HTTPD_CONNECTION_CHUNKED_BODY_UNREADY:
            case HTTPD_CONNECTION_BODY_SENT:
                break;
            case HTTPD_CONNECTION_FOOTERS_SENDING:
                do_write(conn);
                if (HTTPD_CONNECTION_FOOTERS_SENDING != conn->state)
                    break;
                check_write_done(conn, HTTPD_CONNECTION_FOOTERS_SENT);
                break;
            case HTTPD_CONNECTION_FOOTERS_SENT:
                break;
            case HTTPD_CONNECTION_CLOSED:
                return HTTPD_YES;
            case HTTPD_TLS_CONNECTION_INIT:
                break;
            default:
                return HTTPD_YES;
        }
        break;
    }
    
    return HTTPD_YES;
}

httpd_status httpd_connection_handle_idle(struct httpd_connection* conn) {
    struct httpd_daemon* daemon;
    
    daemon = conn->daemon;
    conn->in_idle = 1;
    
    while (1) {
        switch (conn->state) {
            case HTTPD_CONNECTION_INIT:
                
        }
    }
}

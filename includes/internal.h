//
//  internal.h
//  myhttpd
//
//  Created by lastland on 03/01/2017.
//  Copyright © 2017 DeepSpec. All rights reserved.
//

#ifndef internal_h
#define internal_h


#define HTTPD_BUF_INC_SIZE 1024
#define HTTPD_POOL_SIZE_DEFAULT (32 * 1024)

#include "httpd.h"
#include "memorypool.h"

#define MAX(a,b) (((a)<(b)) ? (b) : (a))
#define MIN(a,b) (((a)<(b)) ? (a) : (b))


typedef pthread_t httpd_thread_handle;

typedef ssize_t (*ReceiveCallback) (struct httpd_connection *conn,
                                    void *write_to, size_t max_bytes);

typedef ssize_t (*TransmitCallback) (struct httpd_connection *conn,
                                     const void *write_to, size_t max_bytes);


enum httpd_connectionEventLoopInfo {
    HTTPD_EVENT_LOOP_INFO_READ = 0,
    HTTPD_EVENT_LOOP_INFO_WRITE = 1,
    HTTPD_EVENT_LOOP_INFO_BLOCK = 2,
    HTTPD_EVENT_LOOP_INFO_CLEANUP = 3
};

enum httpd_connection_state {
    /**
     * Connection just started (no headers received).
     * Waiting for the line with the request type, URL and version.
     */
    HTTPD_CONNECTION_INIT = 0,
    
    /**
     * 1: We got the URL (and request type and version).  Wait for a header line.
     */
    HTTPD_CONNECTION_URL_RECEIVED = HTTPD_CONNECTION_INIT + 1,
    
    /**
     * 2: We got part of a multi-line request header.  Wait for the rest.
     */
    HTTPD_CONNECTION_HEADER_PART_RECEIVED = HTTPD_CONNECTION_URL_RECEIVED + 1,
    
    /**
     * 3: We got the request headers.  Process them.
     */
    HTTPD_CONNECTION_HEADERS_RECEIVED = HTTPD_CONNECTION_HEADER_PART_RECEIVED + 1,
    
    /**
     * 4: We have processed the request headers.  Send 100 continue.
     */
    HTTPD_CONNECTION_HEADERS_PROCESSED = HTTPD_CONNECTION_HEADERS_RECEIVED + 1,
    
    /**
     * 5: We have processed the headers and need to send 100 CONTINUE.
     */
    HTTPD_CONNECTION_CONTINUE_SENDING = HTTPD_CONNECTION_HEADERS_PROCESSED + 1,
    
    /**
     * 6: We have sent 100 CONTINUE (or do not need to).  Read the message body.
     */
    HTTPD_CONNECTION_CONTINUE_SENT = HTTPD_CONNECTION_CONTINUE_SENDING + 1,
    
    /**
     * 7: We got the request body.  Wait for a line of the footer.
     */
    HTTPD_CONNECTION_BODY_RECEIVED = HTTPD_CONNECTION_CONTINUE_SENT + 1,
    
    /**
     * 8: We got part of a line of the footer.  Wait for the
     * rest.
     */
    HTTPD_CONNECTION_FOOTER_PART_RECEIVED = HTTPD_CONNECTION_BODY_RECEIVED + 1,
    
    /**
     * 9: We received the entire footer.  Wait for a response to be queued
     * and prepare the response headers.
     */
    HTTPD_CONNECTION_FOOTERS_RECEIVED = HTTPD_CONNECTION_FOOTER_PART_RECEIVED + 1,
    
    /**
     * 10: We have prepared the response headers in the writ buffer.
     * Send the response headers.
     */
    HTTPD_CONNECTION_HEADERS_SENDING = HTTPD_CONNECTION_FOOTERS_RECEIVED + 1,
    
    /**
     * 11: We have sent the response headers.  Get ready to send the body.
     */
    HTTPD_CONNECTION_HEADERS_SENT = HTTPD_CONNECTION_HEADERS_SENDING + 1,
    
    /**
     * 12: We are ready to send a part of a non-chunked body.  Send it.
     */
    HTTPD_CONNECTION_NORMAL_BODY_READY = HTTPD_CONNECTION_HEADERS_SENT + 1,
    
    /**
     * 13: We are waiting for the client to provide more
     * data of a non-chunked body.
     */
    HTTPD_CONNECTION_NORMAL_BODY_UNREADY = HTTPD_CONNECTION_NORMAL_BODY_READY + 1,
    
    /**
     * 14: We are ready to send a chunk.
     */
    HTTPD_CONNECTION_CHUNKED_BODY_READY = HTTPD_CONNECTION_NORMAL_BODY_UNREADY + 1,
    
    /**
     * 15: We are waiting for the client to provide a chunk of the body.
     */
    HTTPD_CONNECTION_CHUNKED_BODY_UNREADY = HTTPD_CONNECTION_CHUNKED_BODY_READY + 1,
    
    /**
     * 16: We have sent the response body. Prepare the footers.
     */
    HTTPD_CONNECTION_BODY_SENT = HTTPD_CONNECTION_CHUNKED_BODY_UNREADY + 1,
    
    /**
     * 17: We have prepared the response footer.  Send it.
     */
    HTTPD_CONNECTION_FOOTERS_SENDING = HTTPD_CONNECTION_BODY_SENT + 1,
    
    /**
     * 18: We have sent the response footer.  Shutdown or restart.
     */
    HTTPD_CONNECTION_FOOTERS_SENT = HTTPD_CONNECTION_FOOTERS_SENDING + 1,
    
    /**
     * 19: This connection is to be closed.
     */
    HTTPD_CONNECTION_CLOSED = HTTPD_CONNECTION_FOOTERS_SENT + 1,
    
    /**
     * 20: This connection is finished (only to be freed)
     */
    HTTPD_CONNECTION_IN_CLEANUP = HTTPD_CONNECTION_CLOSED + 1,
    
    /*
     *  SSL/TLS connection states
     */
    
    /**
     * The initial connection state for all secure connectoins
     * Handshake messages will be processed in this state & while
     * in the #HTTPD_TLS_HELLO_REQUEST state
     */
    HTTPD_TLS_CONNECTION_INIT = HTTPD_CONNECTION_IN_CLEANUP + 1

};

enum HTTPD_ValueKind
{
    
    /**
     * Response header
     */
    HTTPD_RESPONSE_HEADER_KIND = 0,
    
    /**
     * HTTP header.
     */
    HTTPD_HEADER_KIND = 1,
    
    /**
     * Cookies.  Note that the original HTTP header containing
     * the cookie(s) will still be available and intact.
     */
    HTTPD_COOKIE_KIND = 2,
    
    /**
     * POST data.  This is available only if a content encoding
     * supported by HTTPD is used (currently only URL encoding),
     * and only if the posted content fits within the available
     * memory pool.  Note that in that case, the upload data
     * given to the #HTTPD_AccessHandlerCallback will be
     * empty (since it has already been processed).
     */
    HTTPD_POSTDATA_KIND = 4,
    
    /**
     * GET (URI) arguments.
     */
    HTTPD_GET_ARGUMENT_KIND = 8,
    
    /**
     * HTTP footer (only for HTTP 1.1 chunked encodings).
     */
    HTTPD_FOOTER_KIND = 16
};


struct httpd_HTTP_header {
    struct httpd_HTTP_header *next;
    char* header;
    char* value;
    enum HTTPD_ValueKind kind;
};


struct httpd_connection {
    struct sockaddr* addr;
    socklen_t addr_len;
    struct httpd_daemon* daemon;
    httpd_socket socket;
    
    char* method;
    char* version;
    char* url;
    
    char* last;
    char* colon;
    
    struct httpd_response *response;
    
    int (*read_handler)(struct httpd_connection *conn);
    int (*write_handler)(struct httpd_connection *conn);
    int (*idle_handler)(struct httpd_connection *conn);
    
    ReceiveCallback recv_cls;
    TransmitCallback send_cls;
    
    struct httpd_connection* prev;
    struct httpd_connection* next;
    enum httpd_connectionEventLoopInfo event_loop_info;
    
    int in_idle;
    
    struct MemoryPool* pool;
    
    enum httpd_connection_state state;
    int read_closed;
    
    char* read_buffer;
    size_t read_buffer_size;
    size_t read_buffer_offset;
    
    char* write_buffer;
    size_t write_buffer_size;
    size_t write_buffer_offset;
    size_t write_buffer_send_offset;
    size_t write_buffer_append_offset;
    
    uint64_t response_write_position;
    
    struct httpd_HTTP_header *headers_received;
    struct httpd_HTTP_header *headers_received_tail;
    
    uint64_t remaining_upload_size;
    
    httpd_status have_chunked_uploaded;    
    size_t current_chunk_size;
    size_t current_chunk_offset;
    
    httpd_status client_aware;
    
    void* client_context;
    
    size_t continue_message_write_offset;

    unsigned int responseCode;
};


struct httpd_response {
    struct httpd_HTTP_header *first_header;
    char* data;
    uint64_t total_size;
    uint64_t data_start;
    
    size_t data_buffer_size;
    size_t data_size;
    
    ContentReaderCallback crc;
    void* crc_cls;
    ContentReaderFreeCallback crfc;

    
    int fd;
};

struct httpd_daemon {
    httpd_socket socket;
    httpd_thread_handle pid;
    httpd_status shutdown;
    
    unsigned int connections;
    unsigned int connection_limit;
    struct httpd_connection* connections_head;
    struct httpd_connection* connections_tail;
    int at_limit;
    
    size_t pool_size;
    size_t pool_increment;
    
    HTTPD_AccessHandlerCallback default_handler;
    void *default_handler_cls;
};


#endif /* internal_h */

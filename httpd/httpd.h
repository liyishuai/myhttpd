#ifndef httpd_h
#define httpd_h

#define INVALID_SOCKET -1

#include <stdint.h>
#include <netinet/ip.h>

#define HTTPD_YES 0
#define HTTPD_NO -1

#define HTTP_OK 200
#define HTTP_CREATED 201
#define HTTP_ACCEPTED 202
#define HTTP_NON_AUTHORITATIVE_INFORMATION 203
#define HTTP_NO_CONTENT 204
#define HTTP_RESET_CONTENT 205
#define HTTP_PARTIAL_CONTENT 206
#define HTTP_MULTI_STATUS 207

#define HTTP_MULTIPLE_CHOICES 300
#define HTTP_MOVED_PERMANENTLY 301
#define HTTP_FOUND 302
#define HTTP_SEE_OTHER 303
#define HTTP_NOT_MODIFIED 304
#define HTTP_USE_PROXY 305
#define HTTP_SWITCH_PROXY 306
#define HTTP_TEMPORARY_REDIRECT 307

#define HTTP_BAD_REQUEST 400
#define HTTP_UNAUTHORIZED 401
#define HTTP_PAYMENT_REQUIRED 402
#define HTTP_FORBIDDEN 403
#define HTTP_NOT_FOUND 404
#define HTTP_METHOD_NOT_ALLOWED 405
#define HTTP_NOT_ACCEPTABLE 406

#define HTTPD_HTTP_VERSION_1_0 "HTTP/1.0"
#define HTTPD_HTTP_VERSION_1_1 "HTTP/1.1"

#define HTTP_HEADER_EXPECT "Expect"
#define HTTP_HEADER_CONNECTION "Connection"

#define HTTPD_HTTP_METHOD_CONNECT "CONNECT"
#define HTTPD_HTTP_METHOD_DELETE "DELETE"
#define HTTPD_HTTP_METHOD_GET "GET"
#define HTTPD_HTTP_METHOD_HEAD "HEAD"
#define HTTPD_HTTP_METHOD_OPTIONS "OPTIONS"
#define HTTPD_HTTP_METHOD_POST "POST"
#define HTTPD_HTTP_METHOD_PUT "PUT"
#define HTTPD_HTTP_METHOD_PATCH "PATCH"
#define HTTPD_HTTP_METHOD_TRACE "TRACE"

#define HTTP_HEADER_TRANSFER_ENCODING "Transfer-Encoding"

#define HTTPD_ICY_FLAG ((uint32_t)(((uint32_t)1) << 31))

#define HTTPD_CONTENT_READER_END_OF_STREAM SIZE_MAX
#define HTTPD_CONTENT_READER_END_WITH_ERROR (SIZE_MAX - 1)


struct httpd_connection;

struct httpd_daemon;

struct httpd_response;

struct httpd_HTTP_header;

typedef int httpd_socket;

typedef int httpd_status;

typedef struct sockaddr_in httpd_sockaddr;

typedef httpd_status (*HTTPD_AccessHandlerCallback) (void *cls,
                                                     struct httpd_connection *connection,
                                                     const char *url,
                                                     const char *method,
                                                     const char *version,
                                                     const char *upload_data,
                                                     size_t *upload_data_size,
                                                     void **con_cls);

typedef ssize_t (*ContentReaderCallback) (void *cls, uint64_t pos, char *buf, size_t max);

typedef void (*ContentReaderFreeCallback) (void *cls);

enum HTTPD_ResponseMemoryMode
{

    /**
     * Buffer is a persistent (static/global) buffer that won't change
     * for at least the lifetime of the response, MHD should just use
     * it, not free it, not copy it, just keep an alias to it.
     * @ingroup response
     */
    HTTPD_RESPMEM_PERSISTENT,

    /**
     * Buffer is heap-allocated with `malloc()` (or equivalent) and
     * should be freed by MHD after processing the response has
     * concluded (response reference counter reaches zero).
     * @ingroup response
     */
    HTTPD_RESPMEM_MUST_FREE,

    /**
     * Buffer is in transient memory, but not on the heap (for example,
     * on the stack or non-`malloc()` allocated) and only valid during the
     * call to #MHD_create_response_from_buffer.  MHD must make its
     * own private copy of the data for processing.
     * @ingroup response
     */
    HTTPD_RESPMEM_MUST_COPY

};


struct httpd_daemon* create_daemon(uint16_t, HTTPD_AccessHandlerCallback, void*);

void stop_daemon(struct httpd_daemon* daemon);

const char *HTTPD_get_response_header (struct httpd_response *response,
                                       const char *key);

void HTTPD_destroy_response (struct httpd_response *response);

const char *
HTTPD_get_reason_phrase_for (unsigned int code);


typedef ssize_t
(*HTTPD_ContentReaderCallback) (void *cls, uint64_t pos, char *buf, size_t max);

typedef void
(*HTTPD_ContentReaderFreeCallback) (void *cls);

struct httpd_response*
HTTPD_create_response_from_buffer (size_t size,
                                   void *buffer,
                                   enum HTTPD_ResponseMemoryMode mode);

struct httpd_response*
HTTPD_create_response_from_callback (uint64_t size,
                                     size_t block_size,
                                     HTTPD_ContentReaderCallback crc,
                                     void *crc_cls,
                                     HTTPD_ContentReaderFreeCallback crfc);

httpd_status HTTPD_queue_response (struct httpd_connection *conn,
                                   unsigned int status_code,
                                   struct httpd_response *response);


#endif /* httpd_h */

#include "macros.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "httpd.h"
#include "internal.h"
#include "connection.h"
#include "httpd_string.h"

#define HTTP_100_CONTINUE "HTTP/1.1 100 Continue\r\n\r\n"

#define HTTP_HEADER_COOKIE "Cookie"
#define HTTP_HEADER_TRANSFER_ENCODING "Transfer-Encoding"
#define HTTP_HEADER_CONTENT_LENGTH "Content-Length"


static httpd_status try_grow_read_buffer(struct httpd_connection* conn) {
    void* buf;
    size_t new_size;

    if (0 == conn->read_buffer_size)
        new_size = conn->daemon->pool_size / 2;
    else
        new_size = conn->read_buffer_size + HTTPD_BUF_INC_SIZE;
    buf = httpd_pool_reallocate(conn->pool, conn->read_buffer,
                                conn->read_buffer_size, new_size);
    if (NULL == buf)
        return HTTPD_NO;
    conn->read_buffer = buf;
    conn->read_buffer_size = new_size;
    return HTTPD_YES;
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
    if ((pos == conn->read_buffer_offset - 1) &&
        ('\n' != rbuf[pos])) {
        if (conn->read_buffer_offset == conn->read_buffer_size) {
            httpd_status r = try_grow_read_buffer(conn);
            if (HTTPD_NO == r) {
                // transmit
            }
        }
        if (line_len)
            *line_len = 0;
        return NULL;
    }

    if (line_len)
        *line_len = pos;
    if (('\r' == rbuf[pos]) && ('\n' == rbuf[pos + 1])) {
        rbuf[pos] = '\0';
        pos = pos + 1;
    }
    rbuf[pos] = '\0';
    pos = pos + 1;
    conn->read_buffer += pos;
    conn->read_buffer_size -= pos;
    conn->read_buffer_offset -= pos;
    return rbuf;
}

const char* httpd_lookup_connection_value(struct httpd_connection* conn,
                                          enum HTTPD_ValueKind kind,
                                          const char* key) {
    struct httpd_HTTP_header* pos;

    if (NULL == conn)
        return NULL;
    for (pos = conn->headers_received; NULL != pos; pos = pos->next) {
        if ((0 != (pos->kind & kind)) &&
            (key == pos->header ||
             (NULL != pos->header &&
              NULL != key &&
              HTTPD_str_equal_caseless_(key, pos->header))))
            return pos->value;
    }
    return NULL;
}

static httpd_status socket_start_no_buffering(struct httpd_connection* conn) {
    int ret = 1;
    const int on_val = 1;
    if (NULL == conn) return HTTPD_NO;
    ret = setsockopt(conn->socket, IPPROTO_TCP, TCP_NODELAY,
                     (const void*)&on_val, sizeof(on_val));
    if (0 == ret)
        return HTTPD_YES;
    else
        return HTTPD_NO;
}

static httpd_status socket_start_normal_buffering(struct httpd_connection* conn) {
    int ret = 1;
    const int off_val = 0;
    if (NULL == conn) return HTTPD_NO;
    ret = setsockopt(conn->socket, IPPROTO_TCP, TCP_NODELAY,
                     (const void*)&off_val, sizeof(off_val));
    if (0 == ret)
        return HTTPD_YES;
    else
        return HTTPD_NO;
}

static int need_100_continue(struct httpd_connection* conn) {
    const char* expect;
    int ret;

    if (NULL != conn->response) return 0;
    if (NULL == conn->version) return 0;

    ret = HTTPD_str_equal_caseless_(conn->version,
                                    HTTPD_HTTP_VERSION_1_1);
    if (0 == ret) return 0;
    expect = httpd_lookup_connection_value(conn, HTTPD_HEADER_KIND,
                                           HTTP_HEADER_EXPECT);
    if (NULL == expect) return 0;
    ret = HTTPD_str_equal_caseless_(expect, "100 continue");
    if (0 == ret) return 0;
    ret = conn->continue_message_write_offset < strlen(HTTP_100_CONTINUE);
    return ret;
}

static void close_connection(struct httpd_connection* conn) {
    struct httpd_daemon* daemon;

    daemon = conn->daemon;
    conn->state = HTTPD_CONNECTION_CLOSED;
    conn->event_loop_info = HTTPD_EVENT_LOOP_INFO_CLEANUP;
    // TODO: notify daemon
}

static httpd_status keepalive_possible (struct httpd_connection *conn)
{
    const char *end;

    if (NULL == conn->version)
        return HTTPD_NO;
    // TODO: http 1.0?
    end = httpd_lookup_connection_value (conn,
                                         HTTPD_HEADER_KIND,
                                         HTTP_HEADER_CONNECTION);
    if (HTTPD_str_equal_caseless_(conn->version,
                                  HTTPD_HTTP_VERSION_1_1)) {
        if (NULL == end)
            return HTTPD_YES;
        if ( (HTTPD_str_equal_caseless_ (end, "close")) ||
            (HTTPD_str_equal_caseless_ (end, "upgrade")) )
            return HTTPD_NO;
        return HTTPD_YES;
    }
    if (HTTPD_str_equal_caseless_(conn->version,
                                  HTTPD_HTTP_VERSION_1_0)) {
        if (NULL == end)
            return HTTPD_NO;
        if (HTTPD_str_equal_caseless_(end, "Keep-Alive"))
            return HTTPD_YES;
        return HTTPD_NO;
    }
    return HTTPD_NO;
}


static httpd_status parse_initial_message_line(struct httpd_connection* conn,
                                      char* line,
                                      size_t line_len) {
    char* uri;
    char* args;
    char* http_version;

    uri = memchr(line, ' ', line_len);
    if (NULL == uri) return HTTPD_NO;
    uri[0] = '\0';
    conn->method = line;
    uri++;

    while (' ' == uri[0] && (size_t)(uri - line) < line_len)
        uri++;
    if (uri - line == line_len) {
        uri = "";
        conn->version = "";
        args = NULL;
    } else {
        http_version = line + line_len - 1;
        while (' ' == http_version[0] && http_version > uri)
            http_version--;
        while (' ' != http_version[0] && http_version > uri)
            http_version--;
        if (http_version > uri)
        {
            http_version[0] = '\0';
            conn->version = http_version + 1;
            args = memchr(uri, '?', http_version - uri);
        }
        else
        {
            conn->version = "";
            args = memchr(uri, '?', line_len - (uri - line));
        }
    }
    // uri log callback?
    if (NULL != args) {
        args[0] = '\0';
        args++;
        // parse arguments
    }
    // unescape escape sequences?
    conn->url = uri;
    return HTTPD_YES;
}

static void call_connection_handler(struct httpd_connection* conn) {
    size_t processed;
    httpd_status ret;

    if (NULL != conn->response)
        return;
    processed = 0;
    conn->client_aware = HTTPD_YES;
    ret = conn->daemon->default_handler(conn->daemon->default_handler_cls,
                                        conn,
                                        conn->url,
                                        conn->method,
                                        conn->version,
                                        NULL, &processed,
                                        &conn->client_context);
    if (HTTPD_NO == ret)
        close_connection(conn);
}

static httpd_status HTTPD_set_connection_value(struct httpd_connection* conn,
                                               enum HTTPD_ValueKind kind,
                                               const char* key, const char* value) {
    struct httpd_HTTP_header* pos;

    pos = httpd_pool_allocate(conn->pool,
                              sizeof(struct httpd_HTTP_header),
                              HTTPD_YES);
    if (NULL == pos)
        return HTTPD_NO;
    pos->header = (char*)key;
    pos->value = (char*)value;
    pos->kind = kind;
    pos->next = NULL;
    if (NULL == conn->headers_received_tail) {
        conn->headers_received = pos;
        conn->headers_received_tail = pos;
    } else {
        conn->headers_received_tail->next = pos;
        conn->headers_received_tail = pos;
    }
    return HTTPD_YES;
}

static httpd_status connection_add_header(struct httpd_connection* conn,
                                          const char* key, const char* value,
                                          enum HTTPD_ValueKind kind) {
    httpd_status ret;
    ret = HTTPD_set_connection_value(conn, kind, key, value);
    if (HTTPD_NO == ret) {
        // transmit_error_response
        return HTTPD_NO;
    }
    return HTTPD_YES;
}

static httpd_status parse_cookie_header(struct httpd_connection* conn) {
    const char *hdr;
    char *cpy;
    char *pos;
    char *sce;
    char *semicolon;
    char *equals;
    char *ekill;
    char old;
    int quotes;
    httpd_status ret;

    hdr = httpd_lookup_connection_value(conn,
                                        HTTPD_HEADER_KIND,
                                        HTTP_HEADER_COOKIE);
    if (NULL == hdr) return HTTPD_NO;
    cpy = httpd_pool_allocate(conn->pool, strlen(hdr) + 1, HTTPD_YES);
    if (NULL == cpy) {
        // transmit_error_response
        return HTTPD_NO;
    }
    memcpy(cpy, hdr, strlen(hdr) + 1);
    pos = cpy;
    while (NULL != pos) {
        while (' ' == *pos) pos++;
        sce = pos;
        while (((*sce) != '\0') &&
               ((*sce) != ',') && ((*sce) != ';') && ((*sce) != '='))
            sce++;

        ekill = sce - 1;
        while ((*ekill == ' ') && (ekill >= pos))
            *(ekill--) = '\0';
        old = *sce;
        *sce = '\0';
        if (old != '=')
        {
            ret = connection_add_header(conn, pos, "",
                                        HTTPD_COOKIE_KIND);
            if (HTTPD_NO == ret)
                return HTTPD_NO;
            if (old == '\0')
                break;
            pos = sce + 1;
            continue;
        }
        equals = sce + 1;
        quotes = 0;
        semicolon = equals;
        while ( ('\0' != semicolon[0]) &&
               ( (0 != quotes) ||
                ( (';' != semicolon[0]) &&
                 (',' != semicolon[0]) ) ) )
        {
            if ('"' == semicolon[0])
                quotes = (quotes + 1) & 1;
            semicolon++;
        }
        if ('\0' == semicolon[0])
            semicolon = NULL;
        if (NULL != semicolon)
        {
            semicolon[0] = '\0';
            semicolon++;
        }
        /* remove quotes */
        if ( ('"' == equals[0]) &&
            ('"' == equals[strlen (equals) - 1]) )
        {
            equals[strlen (equals) - 1] = '\0';
            equals++;
        }
        ret = connection_add_header(conn, pos, equals,
                                    HTTPD_COOKIE_KIND);
        if (HTTPD_NO == ret)
            return HTTPD_NO;
        pos = semicolon;
    }
    return HTTPD_YES;
}

static void parse_connection_headers(struct httpd_connection* conn) {
    const char* clen;
    const char* enc;
    const char* end;

    parse_cookie_header(conn);
    // pedantic check
    conn->remaining_upload_size = 0;
    enc = httpd_lookup_connection_value(conn, HTTPD_HEADER_KIND,
                                        HTTP_HEADER_TRANSFER_ENCODING);
    if (NULL != enc) {
        conn->remaining_upload_size = UINT64_MAX;
        if (HTTPD_str_equal_caseless_(enc, "chuncked"))
            conn->have_chunked_uploaded = HTTPD_YES;
    } else {
        clen = httpd_lookup_connection_value(conn, HTTPD_HEADER_KIND,
                                             HTTP_HEADER_CONTENT_LENGTH);
        if (NULL != clen) {
            end = clen + HTTPD_str_to_uint64_(clen,
                                              &conn->remaining_upload_size);
            if (clen == end || '\0' != *end) {
                conn->remaining_upload_size = 0;
                close_connection(conn);
                return;
            }
        }
    }
}

static httpd_status process_header_line(struct httpd_connection* conn,
                                        char* line) {
    char* colon;

    colon = strchr(line, ':');
    if (NULL == colon) {
        close_connection(conn);
        return HTTPD_NO;
    }
    colon[0] = '\0';
    colon++;
    while ((colon[0] != '\0') && ((colon[0] == ' ') || (colon[0] == '\t')))
        colon++;
    conn->last = line;
    conn->colon = colon;
    return HTTPD_YES;
}

static httpd_status process_broken_line(struct httpd_connection* conn,
                                        char* line, enum HTTPD_ValueKind kind) {
    char* last;
    char* tmp;
    size_t last_len, tmp_len;

    last = conn->last;
    if (' ' == line[0] || '\t' == line[0]) {
        last_len = strlen(last);
        tmp = line;
        while (' ' == tmp[0] || '\t' == line[0])
            tmp++;
        tmp_len = strlen(tmp);
        last = httpd_pool_reallocate(conn->pool, last,
                                     last_len + 1,
                                     last_len + tmp_len + 1);
        if (NULL == last) {
            // transmit_error_response
            return HTTPD_NO;
        }
        memcpy(&last[last_len], tmp, tmp_len + 1);
        conn->last = last;
        return HTTPD_YES;
    }
    if (HTTPD_NO == connection_add_header(conn, last,
                                          conn->colon,
                                          kind)) {
        // transmit_error_response
        return HTTPD_NO;
    }
    if (0 != line[0]) {
        httpd_status ret;
        ret = process_header_line(conn, line);
        if (HTTPD_NO == ret) {
            // transmit_error_response
            return HTTPD_NO;
        }
    }
    return HTTPD_YES;
}

static void process_request_body(struct httpd_connection* conn) {
    size_t processed;
    size_t available;
    size_t used;
    size_t i;
    httpd_status instant_retry;
    int malformed;
    char *buffer_head;
    httpd_status ret;

    if (NULL != conn->response)
        return;

    buffer_head = conn->read_buffer;
    available = conn->read_buffer_offset;
    do {
        instant_retry = HTTPD_NO;
        if (HTTPD_YES == conn->have_chunked_uploaded &&
            UINT64_MAX == conn->remaining_upload_size) {
            if (conn->current_chunk_offset == conn->current_chunk_size &&
                0 != conn->current_chunk_offset &&
                available >= 2) {
                i = 0;
                if ((buffer_head[i] == '\r') || (buffer_head[i] == '\n'))
                    i++;            /* skip 1st part of line feed */
                if ((buffer_head[i] == '\r') || (buffer_head[i] == '\n'))
                    i++;            /* skip 2nd part of line feed */
                if (0 == i) {
                    close_connection(conn);
                    return;
                }
                available -= i;
                buffer_head += i;
                conn->current_chunk_offset = 0;
                conn->current_chunk_size = 0;
            }
            if (conn->current_chunk_offset < conn->current_chunk_size) {
                processed = conn->current_chunk_size - conn->current_chunk_offset;
                if (processed > available)
                    processed = available;
                if (available > processed)
                    instant_retry = HTTPD_YES;
            } else {
                i = 0;
                while (i < available) {
                    if ((buffer_head[i] == '\r') || (buffer_head[i] == '\n'))
                        break;
                    i++;
                    if (i >= 6)
                        break;
                }
                if ((i + 1 >= available) &&
                    !((i == 1) && (available == 2) && (buffer_head[0] == '0')))
                    break;          /* need more data... */
                malformed = (i >= 6);
                if (!malformed)
                {
                    size_t num_dig;
                    num_dig = HTTPD_strx_to_sizet_n_(buffer_head, i,
                                                     &conn->current_chunk_size);
                    malformed = (i != num_dig);
                }
                if (malformed)
                {
                    close_connection(conn);
                    return;
                }
                i++;
                if (i < available && (buffer_head[i] == '\r' || buffer_head[i] == '\n')) {
                    i++;       /* skip 2nd part of line feed */
                }
                buffer_head += i;
                available -= i;
                conn->current_chunk_offset = 0;
                if (available > 0)
                    instant_retry = HTTPD_YES;
                if (0 == conn->current_chunk_size)
                {
                    conn->remaining_upload_size = 0;
                    break;
                }
                continue;
            }
        } else {
            /* no chunked encoding, give all to the client */
            if ( (0 != conn->remaining_upload_size) &&
                (UINT64_MAX != conn->remaining_upload_size) &&
                (conn->remaining_upload_size < available) )
            {
                processed = (size_t)conn->remaining_upload_size;
            }
            else
            {
                /**
                 * 1. no chunked encoding, give all to the client
                 * 2. client may send large chunked data, but only a smaller part is available at one time.
                 */
                processed = available;
            }
        }
        used = processed;
        conn->client_aware = HTTPD_YES;
        ret = conn->daemon->default_handler(conn->daemon->default_handler_cls,
                                            conn,
                                            conn->url,
                                            conn->method,
                                            conn->version,
                                            buffer_head,
                                            &processed,
                                            &conn->client_context);
        if (HTTPD_NO == ret) {
            close_connection(conn);
            return;
        }
        if (processed > used) {
            // panic?
            exit(1);
        }
        if (0 != processed)
            instant_retry = HTTPD_NO;
        used -= processed;
        if (HTTPD_YES == conn->have_chunked_uploaded)
            conn->current_chunk_offset += used;
        buffer_head += used;
        available -= used;
        if (UINT64_MAX != conn->remaining_upload_size)
            conn->remaining_upload_size -= used;
    } while (HTTPD_YES == instant_retry);
    if (available > 0)
        memmove (conn->read_buffer, buffer_head, available);
    conn->read_buffer_offset = available;
}

static httpd_status build_header_response(struct httpd_connection* conn) {
    size_t size;
    size_t off;
    struct httpd_HTTP_header *pos;
    char code[256];
    char date[128];
    char content_length_buf[128];
    size_t content_length_len;
    char *data;
    enum HTTPD_ValueKind kind;
    const char *reason_phrase;
    uint32_t rc;
    const char *client_requested_close;
    const char *response_has_close;
    const char *response_has_keepalive;
    const char *have_encoding;
    const char *have_content_length;
    int must_add_close;
    int must_add_chunked_encoding;
    int must_add_keep_alive;
    int must_add_content_length;
    int r;

    if (0 == conn->version[0]) {
        data = httpd_pool_allocate (conn->pool, 0, HTTPD_YES);
        conn->write_buffer = data;
        conn->write_buffer_append_offset = 0;
        conn->write_buffer_send_offset = 0;
        conn->write_buffer_size = 0;
        return HTTPD_YES;
    }
    rc = conn->responseCode & ~HTTPD_ICY_FLAG;
    if (HTTPD_CONNECTION_FOOTERS_RECEIVED == conn->state)
    {
        reason_phrase = HTTPD_get_reason_phrase_for (rc);
        sprintf (code,
                 "%s %u %s\r\n",
                 (0 != (conn->responseCode & HTTPD_ICY_FLAG))
                 ? "ICY"
                 : ( (HTTPD_str_equal_caseless_ (HTTPD_HTTP_VERSION_1_0,
                                                 conn->version))
                    ? HTTPD_HTTP_VERSION_1_0
                    : HTTPD_HTTP_VERSION_1_1),
                 rc,
                 reason_phrase);
        off = strlen (code);
        /* estimate size */
        size = off + 2;           /* +2 for extra "\r\n" at the end */
        kind = HTTPD_HEADER_KIND;
        date[0] = '\0';
        size += strlen (date);
    }
    else
    {
        /* 2 bytes for final CRLF of a Chunked-Body */
        size = 2;
        kind = HTTPD_FOOTER_KIND;
        off = 0;
    }

    /* calculate extra headers we need to add, such as 'Connection: close',
     first see what was explicitly requested by the application */
    must_add_close = HTTPD_NO;
    must_add_chunked_encoding = HTTPD_NO;
    must_add_keep_alive = HTTPD_NO;
    must_add_content_length = HTTPD_NO;
    switch (conn->state)
    {
        case HTTPD_CONNECTION_FOOTERS_RECEIVED:
            response_has_close = HTTPD_get_response_header (conn->response,
                                                            HTTP_HEADER_CONNECTION);
            response_has_keepalive = response_has_close;
            if (NULL != response_has_close) {
                r = HTTPD_str_equal_caseless_(response_has_close, "close");
                if (!r)
                    response_has_close = NULL;
            }
            if (NULL != response_has_keepalive) {
                r = HTTPD_str_equal_caseless_(response_has_keepalive, "Keep-Alive");
                if (!r)
                    response_has_keepalive = NULL;
            }
            client_requested_close = httpd_lookup_connection_value(conn,
                                                                   HTTPD_HEADER_KIND,
                                                                   HTTP_HEADER_CONNECTION);
            if (NULL != client_requested_close) {
                r = HTTPD_str_equal_caseless_(client_requested_close, "close");
                if (!r)
                    client_requested_close = NULL;
            }

            /* now analyze chunked encoding situation */
            conn->have_chunked_uploaded = HTTPD_NO;

            if ( (UINT64_MAX == conn->response->total_size) &&
                (NULL == response_has_close) &&
                (NULL == client_requested_close) )
            {
                /* size is unknown, and close was not explicitly requested;
                 need to either to HTTP 1.1 chunked encoding or
                 close the connection */
                /* 'close' header doesn't exist yet, see if we need to add one;
                 if the client asked for a close, no need to start chunk'ing */
                if (HTTPD_YES == keepalive_possible (conn)) {
                    r = HTTPD_str_equal_caseless_(HTTPD_HTTP_VERSION_1_1,
                                                  conn->version);
                    if (r) {
                        have_encoding = HTTPD_get_response_header(conn->response,
                                                                  HTTP_HEADER_TRANSFER_ENCODING);
                        if (NULL == have_encoding) {
                            must_add_chunked_encoding = HTTPD_YES;
                            conn->have_chunked_uploaded = HTTPD_YES;
                        } else {
                            r = HTTPD_str_equal_caseless_(have_encoding, "identity");
                            if (r)
                                must_add_close = HTTPD_YES;
                            else
                                conn->have_chunked_uploaded = HTTPD_YES;
                        }
                    }
                } else {
                    /* Keep alive or chunking not possible
                     => set close header if not present */
                    if (NULL == response_has_close)
                        must_add_close = HTTPD_YES;
                }
            }

            /* check for other reasons to add 'close' header */
            // TODO

            /* check if we should add a 'content length' header */
            have_content_length = HTTPD_get_response_header (conn->response,
                                                             HTTP_HEADER_CONTENT_LENGTH);

            /* MHD_HTTP_NO_CONTENT, MHD_HTTP_NOT_MODIFIED and 1xx-status
             codes SHOULD NOT have a Content-Length according to spec;
             also chunked encoding / unknown length or CONNECT... */
            if ( (UINT64_MAX != conn->response->total_size) &&
                (HTTP_NO_CONTENT != rc) &&
                (HTTP_NOT_MODIFIED != rc) &&
                (HTTP_OK <= rc) &&
                (NULL == have_content_length) &&
                ( (NULL == conn->method) ||
                 (! HTTPD_str_equal_caseless_(conn->method,
                                               HTTPD_HTTP_METHOD_CONNECT)) ) )
            {
                /*
                 Here we add a content-length if one is missing; however,
                 for 'connect' methods, the responses MUST NOT include a
                 content-length header *if* the response code is 2xx (in
                 which case we expect there to be no body).  Still,
                 as we don't know the response code here in some cases, we
                 simply only force adding a content-length header if this
                 is not a 'connect' or if the response is not empty
                 (which is kind of more sane, because if some crazy
                 application did return content with a 2xx status code,
                 then having a content-length might again be a good idea).

                 Note that the change from 'SHOULD NOT' to 'MUST NOT' is
                 a recent development of the HTTP 1.1 specification.
                 */
                content_length_len
                = sprintf (content_length_buf,
                           HTTP_HEADER_CONTENT_LENGTH ": %llu\r\n",
                           (unsigned long long) conn->response->total_size);
                must_add_content_length = HTTPD_YES;
            }

            /* check for adding keep alive */
            if ( (NULL == response_has_keepalive) &&
                (NULL == response_has_close) &&
                (HTTPD_NO == must_add_close) &&
                (HTTPD_YES == keepalive_possible (conn)) )
                must_add_keep_alive = HTTPD_YES;
            break;
        case HTTPD_CONNECTION_BODY_SENT:
            response_has_keepalive = NULL;
            break;
        default:
            break;
    }

    if (HTTPD_YES == must_add_close)
        size += strlen ("Connection: close\r\n");
    if (HTTPD_YES == must_add_keep_alive)
        size += strlen ("Connection: Keep-Alive\r\n");
    if (HTTPD_YES == must_add_chunked_encoding)
        size += strlen ("Transfer-Encoding: chunked\r\n");
    if (HTTPD_YES == must_add_content_length)
        size += content_length_len;
    // TODO: extra check

    for (pos = conn->response->first_header; NULL != pos; pos = pos->next)
        if ( (pos->kind == kind) &&
            (! ( (HTTPD_YES == must_add_close) &&
                (pos->value == response_has_keepalive) &&
                (HTTPD_str_equal_caseless_(pos->header,
                                           HTTP_HEADER_CONNECTION) ) ) ) )
            size += strlen (pos->header) + strlen (pos->value) + 4; /* colon, space, linefeeds */
    /* produce data */
    data = httpd_pool_allocate (conn->pool, size + 1, HTTPD_NO);
    if (NULL == data)
    {
        return HTTPD_NO;
    }
    if (HTTPD_CONNECTION_FOOTERS_RECEIVED == conn->state)
    {
        memcpy (data, code, off);
    }
    if (HTTPD_YES == must_add_close)
    {
        /* we must add the 'Connection: close' header */
        memcpy (&data[off],
                "Connection: close\r\n",
                strlen ("Connection: close\r\n"));
        off += strlen ("Connection: close\r\n");
    }
    if (HTTPD_YES == must_add_keep_alive)
    {
        /* we must add the 'Connection: Keep-Alive' header */
        memcpy (&data[off],
                "Connection: Keep-Alive\r\n",
                strlen ("Connection: Keep-Alive\r\n"));
        off += strlen ("Connection: Keep-Alive\r\n");
    }
    if (HTTPD_YES == must_add_chunked_encoding)
    {
        /* we must add the 'Transfer-Encoding: chunked' header */
        memcpy (&data[off],
                "Transfer-Encoding: chunked\r\n",
                strlen ("Transfer-Encoding: chunked\r\n"));
        off += strlen ("Transfer-Encoding: chunked\r\n");
    }
    if (HTTPD_YES == must_add_content_length)
    {
        /* we must add the 'Content-Length' header */
        memcpy (&data[off],
                content_length_buf,
                content_length_len);
        off += content_length_len;
    }
    for (pos = conn->response->first_header; NULL != pos; pos = pos->next)
        if ( (pos->kind == kind) &&
            (! ( (pos->value == response_has_keepalive) &&
                (HTTPD_YES == must_add_close) &&
                (HTTPD_str_equal_caseless_(pos->header,
                                           HTTP_HEADER_CONNECTION) ) ) ) )
            off += sprintf (&data[off],
                            "%s: %s\r\n",
                            pos->header,
                            pos->value);
    if (HTTPD_CONNECTION_FOOTERS_RECEIVED == conn->state)
    {
        strcpy (&data[off], date);
        off += strlen (date);
    }
    memcpy (&data[off], "\r\n", 2);
    off += 2;

    if (off != size) {
        exit(1);
        // TODO: panic
    }

    conn->write_buffer = data;
    conn->write_buffer_append_offset = size;
    conn->write_buffer_send_offset = 0;
    conn->write_buffer_size = size + 1;
    return HTTPD_YES;
}

static void cleanup_connection(struct httpd_connection* conn) {
    // TODO: everything!
    close_connection(conn);
    conn->in_idle = 0;
}

static httpd_status try_ready_normal_body(struct httpd_connection* conn) {
    ssize_t ret;
    struct httpd_response* response;

    response = conn->response;
    if (NULL == response->crc)
        return HTTPD_YES;
    if (0 == response->total_size ||
        conn->response_write_position == response->total_size)
        return HTTPD_YES; /* 0-byte response is always ready */
    if (response->data_start <= conn->response_write_position &&
        response->data_size + response->data_start >
        conn->response_write_position)
        return HTTPD_YES; /* response already ready */
    ret = response->crc (response->crc_cls,
                         conn->response_write_position,
                         response->data,
                         (size_t)MIN ((uint64_t)response->data_buffer_size,
                                      response->total_size -
                                      conn->response_write_position));
    if ( (((ssize_t) HTTPD_CONTENT_READER_END_OF_STREAM) == ret) ||
        (((ssize_t) HTTPD_CONTENT_READER_END_WITH_ERROR) == ret) )
    {
        /* either error or http 1.0 transfer, close socket! */
        response->total_size = conn->response_write_position;
        close_connection(conn);
        return HTTPD_NO;
    }

    response->data_start = conn->response_write_position;
    response->data_size = ret;
    if (0 == ret)
    {
        conn->state = HTTPD_CONNECTION_NORMAL_BODY_UNREADY;
        return HTTPD_NO;
    }
    return HTTPD_YES;
}

static httpd_status try_ready_chunked_body(struct httpd_connection* conn) {
    ssize_t ret;
    char *buf;
    struct httpd_response *response;
    size_t size;
    char cbuf[10];                /* 10: max strlen of "%x\r\n" */
    int cblen;

    response = conn->response;
    if (0 == conn->write_buffer_size) {
        size = MIN(conn->daemon->pool_size,
                   2 * (0xFFFFFF + sizeof(cbuf) + 2));
        do {
            size /= 2;
            if (size < 128) {
                /* not enough memory */
                close_connection(conn);
                return HTTPD_NO;
            }
            buf = httpd_pool_allocate (conn->pool, size, HTTPD_NO);
        }
        while (NULL == buf);
        conn->write_buffer_size = size;
        conn->write_buffer = buf;
    }
    if (0 == response->total_size)
        ret = 0; /* response must be empty, don't bother calling crc */
    else if ( (response->data_start <=
               conn->response_write_position) &&
             (response->data_start + response->data_size >
              conn->response_write_position) ) {
        /* difference between response_write_position and data_start is less
         than data_size which is size_t type, no need to check for overflow */
        const size_t data_write_offset =
        (size_t)(conn->response_write_position - response->data_start);
        /* buffer already ready, use what is there for the chunk */
        ret = response->data_size - data_write_offset;
        if ( ((size_t) ret) > conn->write_buffer_size - sizeof (cbuf) - 2 )
            ret = conn->write_buffer_size - sizeof (cbuf) - 2;
        memcpy (&conn->write_buffer[sizeof (cbuf)],
                &response->data[data_write_offset], ret);
    } else {
        /* buffer not in range, try to fill it */
        ret = response->crc (response->crc_cls,
                             conn->response_write_position,
                             &conn->write_buffer[sizeof (cbuf)],
                             conn->write_buffer_size - sizeof (cbuf) - 2);
    }
    if ( ((ssize_t) HTTPD_CONTENT_READER_END_WITH_ERROR) == ret)
    {
        /* error, close socket! */
        response->total_size = conn->response_write_position;
        close_connection(conn);
        return HTTPD_NO;
    }
    if ( (((ssize_t) HTTPD_CONTENT_READER_END_OF_STREAM) == ret) ||
        (0 == response->total_size) )
    {
        /* end of message, signal other side! */
        strcpy (conn->write_buffer, "0\r\n");
        conn->write_buffer_append_offset = 3;
        conn->write_buffer_send_offset = 0;
        response->total_size = conn->response_write_position;
        return HTTPD_YES;
    }
    if (0 == ret)
    {
        conn->state = HTTPD_CONNECTION_CHUNKED_BODY_UNREADY;
        return HTTPD_NO;
    }
    if (ret > 0xFFFFFF)
        ret = 0xFFFFFF;
    cblen = snprintf(cbuf, sizeof (cbuf),
                     "%X\r\n", (unsigned int) ret);
    memcpy (&conn->write_buffer[sizeof (cbuf) - cblen], cbuf, cblen);
    memcpy (&conn->write_buffer[sizeof (cbuf) + ret], "\r\n", 2);
    conn->response_write_position += ret;
    conn->write_buffer_send_offset = sizeof (cbuf) - cblen;
    conn->write_buffer_append_offset = sizeof (cbuf) + ret + 2;
    return HTTPD_YES;
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
    printf("%s\n", conn->write_buffer);
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
    httpd_status r;

    if (HTTPD_CONNECTION_CLOSED == conn->state)
        return HTTPD_YES;
    if (conn->read_buffer_offset + conn->daemon->pool_increment >
        conn->read_buffer_size)
        try_grow_read_buffer(conn);
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
                httpd_pool_reallocate(conn->pool, conn->read_buffer,
                                      conn->read_buffer_size + 1,
                                      conn->read_buffer_offset);
                break;
        }
        break;
    }
    return HTTPD_YES;
}

static void HTTPD_connection_update_event_loop_info(struct httpd_connection* conn) {
    httpd_status ret;

    while (1) {
        switch (conn->state) {
            case HTTPD_CONNECTION_INIT:
            case HTTPD_CONNECTION_URL_RECEIVED:
            case HTTPD_CONNECTION_HEADER_PART_RECEIVED:
                /* while reading headers, we always grow the
                 read buffer if needed, no size-check required */
                if ( (conn->read_buffer_offset == conn->read_buffer_size) &&
                    (HTTPD_NO == try_grow_read_buffer (conn)) )
                {
                    // transmit_error_response
                    continue;
                }
                if (!conn->read_closed)
                    conn->event_loop_info = HTTPD_EVENT_LOOP_INFO_READ;
                else
                    conn->event_loop_info = HTTPD_EVENT_LOOP_INFO_BLOCK;
                break;
            case HTTPD_CONNECTION_HEADERS_RECEIVED:
                break;
            case HTTPD_CONNECTION_HEADERS_PROCESSED:
                break;
            case HTTPD_CONNECTION_CONTINUE_SENDING:
                conn->event_loop_info = HTTPD_EVENT_LOOP_INFO_WRITE;
                break;
            case HTTPD_CONNECTION_CONTINUE_SENT:
                if (conn->read_buffer_offset == conn->read_buffer_size)
                {
                    ret = try_grow_read_buffer(conn);
                    if (HTTPD_YES != ret) {
                        /* failed to grow the read buffer, and the
                         client which is supposed to handle the
                         received data in a *blocking* fashion
                         (in this mode) did not handle the data as
                         it was supposed to!
                         => we would either have to do busy-waiting
                         (on the client, which would likely fail),
                         or if we do nothing, we would just timeout
                         on the connection (if a timeout is even
                         set!).
                         Solution: we kill the connection with an error */
                        // transmit_error_response
                        continue;
                    }
                }
                if ( (conn->read_buffer_offset < conn->read_buffer_size) &&
                    (!conn->read_closed) )
                    conn->event_loop_info = HTTPD_EVENT_LOOP_INFO_READ;
                else
                    conn->event_loop_info = HTTPD_EVENT_LOOP_INFO_BLOCK;
                break;
            case HTTPD_CONNECTION_BODY_RECEIVED:
            case HTTPD_CONNECTION_FOOTER_PART_RECEIVED:
                /* while reading footers, we always grow the
                 read buffer if needed, no size-check required */
                if (conn->read_closed)
                {
                    close_connection(conn);
                    continue;
                }
                conn->event_loop_info = HTTPD_EVENT_LOOP_INFO_READ;
                /* transition to FOOTERS_RECEIVED
                 happens in read handler */
                break;
            case HTTPD_CONNECTION_FOOTERS_RECEIVED:
                conn->event_loop_info = HTTPD_EVENT_LOOP_INFO_BLOCK;
                break;
            case HTTPD_CONNECTION_HEADERS_SENDING:
                /* headers in buffer, keep writing */
                conn->event_loop_info = HTTPD_EVENT_LOOP_INFO_WRITE;
                break;
            case HTTPD_CONNECTION_HEADERS_SENT:
                break;
            case HTTPD_CONNECTION_NORMAL_BODY_READY:
                conn->event_loop_info = HTTPD_EVENT_LOOP_INFO_WRITE;
                break;
            case HTTPD_CONNECTION_NORMAL_BODY_UNREADY:
                conn->event_loop_info = HTTPD_EVENT_LOOP_INFO_BLOCK;
                break;
            case HTTPD_CONNECTION_CHUNKED_BODY_READY:
                conn->event_loop_info = HTTPD_EVENT_LOOP_INFO_WRITE;
                break;
            case HTTPD_CONNECTION_CHUNKED_BODY_UNREADY:
                conn->event_loop_info = HTTPD_EVENT_LOOP_INFO_BLOCK;
                break;
            case HTTPD_CONNECTION_BODY_SENT:
                break;
            case HTTPD_CONNECTION_FOOTERS_SENDING:
                conn->event_loop_info = HTTPD_EVENT_LOOP_INFO_WRITE;
                break;
            case HTTPD_CONNECTION_FOOTERS_SENT:
                break;
            case HTTPD_CONNECTION_CLOSED:
                conn->event_loop_info = HTTPD_EVENT_LOOP_INFO_CLEANUP;
                return;       /* do nothing, not even reading */
            default:
                break;
        }
        break;
    }
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
                    ret = try_ready_normal_body(conn);
                    if (HTTPD_YES != ret)
                        break;
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
    size_t line_len;
    char* line;
    httpd_status r;
    const char *end;
    int client_close;

    daemon = conn->daemon;
    conn->in_idle = 1;

    while (1) {
        switch (conn->state) {
            case HTTPD_CONNECTION_INIT:
                line = get_next_header_line(conn, &line_len);
                if ((NULL == line) || (0 == line[0])) {
                    if (HTTPD_CONNECTION_INIT != conn->state)
                        continue;
                    if (conn->read_closed) {
                        close_connection(conn);
                        continue;
                    }
                    break;
                }
                r = parse_initial_message_line(conn, line, line_len);
                if (HTTPD_NO == r)
                    close_connection(conn);
                else
                    conn->state = HTTPD_CONNECTION_URL_RECEIVED;
                continue;
            case HTTPD_CONNECTION_URL_RECEIVED:
                line = get_next_header_line(conn, &line_len);
                if (NULL == line) {
                    if (HTTPD_CONNECTION_URL_RECEIVED != conn->state)
                        continue;
                    if (conn->read_closed) {
                        close_connection(conn);
                        continue;
                    }
                    break;
                }
                if (0 == line[0]) {
                    conn->state = HTTPD_CONNECTION_HEADERS_RECEIVED;
                    continue;
                }
                r = process_header_line(conn, line);
                if (HTTPD_NO == r) {
                    // TODO: transimit error response?
                    break;
                }
                conn->state = HTTPD_CONNECTION_HEADER_PART_RECEIVED;
                continue;
            case HTTPD_CONNECTION_HEADER_PART_RECEIVED:
                line = get_next_header_line(conn, &line_len);
                if (NULL == line) {
                    if (HTTPD_CONNECTION_HEADER_PART_RECEIVED != conn->state)
                        continue;
                    if (conn->read_closed) {
                        close_connection(conn);
                        continue;
                    }
                    break;
                }
                r = process_broken_line(conn, line, HTTPD_HEADER_KIND);
                if (HTTPD_NO == r)
                    continue;
                if (0 == line[0]) {
                    conn->state = HTTPD_CONNECTION_HEADERS_RECEIVED;
                    continue;
                }
                continue;
            case HTTPD_CONNECTION_HEADERS_RECEIVED:
                parse_connection_headers(conn);
                if (HTTPD_CONNECTION_CLOSED == conn->state)
                    continue;
                conn->state = HTTPD_CONNECTION_HEADERS_PROCESSED;
                continue;
            case HTTPD_CONNECTION_HEADERS_PROCESSED:
                call_connection_handler(conn);
                if (HTTPD_CONNECTION_CLOSED == conn->state)
                    continue;
                r = need_100_continue(conn);
                if (r) {
                    conn->state = HTTPD_CONNECTION_CONTINUE_SENDING;
                    socket_start_no_buffering(conn);
                    break;
                }
                if (NULL != conn->response) {
                    r = HTTPD_str_equal_caseless_(conn->method,
                                                  HTTPD_HTTP_METHOD_POST);
                    if (!r)
                        r = HTTPD_str_equal_caseless_(conn->method,
                                                      HTTPD_HTTP_METHOD_PUT);
                    if (r) {
                        conn->remaining_upload_size = 0;
                        conn->read_closed = 1;
                    }
                }
                if (0 == conn->remaining_upload_size)
                    conn->state = HTTPD_CONNECTION_FOOTERS_RECEIVED;
                else
                    conn->state = HTTPD_CONNECTION_CONTINUE_SENT;
                continue;
            case HTTPD_CONNECTION_CONTINUE_SENDING:
                if (strlen(HTTP_100_CONTINUE) == conn->continue_message_write_offset) {
                    conn->state = HTTPD_CONNECTION_CONTINUE_SENT;
                    socket_start_normal_buffering(conn);
                    continue;
                }
                break;
            case HTTPD_CONNECTION_CONTINUE_SENT:
                if (0 != conn->read_buffer_offset) {
                    process_request_body(conn);
                    if (HTTPD_CONNECTION_CLOSED == conn->state)
                        continue;
                }
                if ((0 == conn->remaining_upload_size) ||
                    ((conn->remaining_upload_size == UINT64_MAX) &&
                     (0 == conn->read_buffer_offset) &&
                     conn->read_closed))
                {
                    if ((HTTPD_YES == conn->have_chunked_uploaded) &&
                        (!conn->read_closed))
                        conn->state = HTTPD_CONNECTION_BODY_RECEIVED;
                    else
                        conn->state = HTTPD_CONNECTION_FOOTERS_RECEIVED;
                    continue;
                }
                break;
            case HTTPD_CONNECTION_BODY_RECEIVED:
                line = get_next_header_line(conn, NULL);
                if (NULL == line)
                {
                    if (conn->state != HTTPD_CONNECTION_BODY_RECEIVED)
                        continue;
                    if (conn->read_closed)
                    {
                        close_connection(conn);
                        continue;
                    }
                    break;
                }
                if (0 == line[0])
                {
                    conn->state = HTTPD_CONNECTION_FOOTERS_RECEIVED;
                    continue;
                }
                r = process_header_line(conn, line);
                if (HTTPD_NO == r)
                {
                    // transmit_error_response
                    break;
                }
                conn->state = HTTPD_CONNECTION_FOOTER_PART_RECEIVED;
                continue;
            case HTTPD_CONNECTION_FOOTER_PART_RECEIVED:
                line = get_next_header_line (conn, NULL);
                if (NULL == line)
                {
                    if (HTTPD_CONNECTION_FOOTER_PART_RECEIVED != conn->state)
                        continue;
                    if (conn->read_closed)
                    {
                        close_connection(conn);
                        continue;
                    }
                    break;
                }
                r = process_broken_line(conn, line, HTTPD_FOOTER_KIND);
                if (HTTPD_YES == r)
                    continue;
                if (0 == line[0])
                {
                    conn->state = HTTPD_CONNECTION_FOOTERS_RECEIVED;
                    continue;
                }
                continue;
            case HTTPD_CONNECTION_FOOTERS_RECEIVED:
                call_connection_handler (conn); /* "final" call */
                if (conn->state == HTTPD_CONNECTION_CLOSED)
                    continue;
                if (NULL == conn->response)
                    break;              /* try again next time */
                r = build_header_response(conn);
                if (HTTPD_NO == r)
                {
                    /* oops - close! */
                    close_connection(conn);
                    continue;
                }
                conn->state = HTTPD_CONNECTION_HEADERS_SENDING;
                socket_start_no_buffering (conn);
                break;
            case HTTPD_CONNECTION_HEADERS_SENDING:
                /* no default action */
                break;
            case HTTPD_CONNECTION_HEADERS_SENT:
                /* Some clients may take some actions right after header receive */
                socket_start_normal_buffering (conn);
                if (HTTPD_YES == conn->have_chunked_uploaded)
                    conn->state = HTTPD_CONNECTION_CHUNKED_BODY_UNREADY;
                else
                    conn->state = HTTPD_CONNECTION_NORMAL_BODY_UNREADY;
                continue;
            case HTTPD_CONNECTION_NORMAL_BODY_READY:
                /* nothing to do here */
                break;
            case HTTPD_CONNECTION_NORMAL_BODY_UNREADY:
                // crc? mutex?
                if (0 == conn->response->total_size)
                {
                    // crc? mutex?
                    conn->state = HTTPD_CONNECTION_BODY_SENT;
                    continue;
                }
                r = try_ready_normal_body(conn);
                if (HTTPD_YES == r)
                {
                    // crc? mutex?
                    conn->state = HTTPD_CONNECTION_NORMAL_BODY_READY;
                    /* Buffering for flushable socket was already enabled*/
                    socket_start_no_buffering (conn);
                    break;
                }
                /* not ready, no socket action */
                break;
            case HTTPD_CONNECTION_CHUNKED_BODY_READY:
                /* nothing to do here */
                break;
            case HTTPD_CONNECTION_CHUNKED_BODY_UNREADY:
                // crc? mutex?
                if ( (0 == conn->response->total_size) ||
                    (conn->response_write_position ==
                     conn->response->total_size) )
                {
                    // crc? mutex?
                    conn->state = HTTPD_CONNECTION_BODY_SENT;
                    continue;
                }
                r = try_ready_chunked_body(conn);
                if (HTTPD_YES == r)
                {
                    // crc? mutex?
                    conn->state = HTTPD_CONNECTION_CHUNKED_BODY_READY;
                    /* Buffering for flushable socket was already enabled */
                    socket_start_no_buffering (conn);

                    continue;
                }
                // crc? mutex?
                break;
            case HTTPD_CONNECTION_BODY_SENT:
                r = build_header_response(conn);
                if (HTTPD_NO == r)
                {
                    /* oops - close! */
                    close_connection(conn);
                    continue;
                }
                if ( (HTTPD_NO == conn->have_chunked_uploaded) ||
                    (conn->write_buffer_send_offset ==
                     conn->write_buffer_append_offset) )
                    conn->state = HTTPD_CONNECTION_FOOTERS_SENT;
                else
                    conn->state = HTTPD_CONNECTION_FOOTERS_SENDING;
                continue;
            case HTTPD_CONNECTION_FOOTERS_SENDING:
                /* no default action */
                break;
            case HTTPD_CONNECTION_FOOTERS_SENT:
                socket_start_normal_buffering (conn);
                end = HTTPD_get_response_header (conn->response,
                                                 HTTP_HEADER_CONNECTION);
                client_close = 0;
                if (NULL != end) {
                    client_close = HTTPD_str_equal_caseless_(end, "close");
                }
                HTTPD_destroy_response (conn->response);
                conn->response = NULL;
                // daemon->notify_completed
                end = httpd_lookup_connection_value (conn,
                                                     HTTPD_HEADER_KIND,
                                                     HTTP_HEADER_CONNECTION);
                if (conn->read_closed ||
                    client_close ||
                    ( (NULL != end) &&
                     (HTTPD_str_equal_caseless_ (end, "close")) ) )
                {
                    conn->read_closed = 1;
                    conn->read_buffer_offset = 0;
                }
                if ((conn->read_closed &&
                     0 == conn->read_buffer_offset) ||
                    (HTTPD_NO == keepalive_possible (conn)))
                {
                    /* have to close for some reason */
                    close_connection(conn);
                    httpd_pool_destroy (conn->pool);
                    conn->pool = NULL;
                    conn->read_buffer = NULL;
                    conn->read_buffer_size = 0;
                    conn->read_buffer_offset = 0;
                }
                else
                {
                    /* can try to keep-alive */
                    conn->version = NULL;
                    conn->state = HTTPD_CONNECTION_INIT;
                    /* Reset the read buffer to the starting size,
                     preserving the bytes we have already read. */
                    conn->read_buffer = httpd_pool_reset (conn->pool,
                                                          conn->read_buffer,
                                                          conn->read_buffer_offset,
                                                          conn->daemon->pool_size / 2);
                    conn->read_buffer_size = conn->daemon->pool_size / 2;
                }
                conn->client_aware = HTTPD_NO;
                conn->client_context = NULL;
                conn->continue_message_write_offset = 0;
                conn->responseCode = 0;
                conn->headers_received = NULL;
                conn->headers_received_tail = NULL;
                conn->response_write_position = 0;
                conn->have_chunked_uploaded = HTTPD_NO;
                conn->method = NULL;
                conn->url = NULL;
                conn->write_buffer = NULL;
                conn->write_buffer_size = 0;
                conn->write_buffer_send_offset = 0;
                conn->write_buffer_append_offset = 0;
                continue;
            case HTTPD_CONNECTION_CLOSED:
                cleanup_connection(conn);
                return HTTPD_NO;
            default:
                break;
        }
        break;
    }
    // time out?
    HTTPD_connection_update_event_loop_info(conn);
    return HTTPD_YES;
}

httpd_status HTTPD_queue_response (struct httpd_connection *conn,
                                   unsigned int status_code,
                                   struct httpd_response *response)
{
    int ret;

    if ( (NULL == conn) ||
        (NULL == response) ||
        (NULL != conn->response) ||
        ( (HTTPD_CONNECTION_HEADERS_PROCESSED != conn->state) &&
         (HTTPD_CONNECTION_FOOTERS_RECEIVED != conn->state) ) )
        return HTTPD_NO;
    //HTTPD_increment_response_rc (response);
    conn->response = response;
    conn->responseCode = status_code;
    ret = HTTPD_str_equal_caseless_(conn->method,
                                    HTTPD_HTTP_METHOD_HEAD);
    if ( ( (NULL != conn->method) && ret ) ||
        (HTTP_OK > status_code) ||
        (HTTP_NO_CONTENT == status_code) ||
        (HTTP_NOT_MODIFIED == status_code) )
    {
        /* if this is a "HEAD" request, or a status code for
         which a body is not allowed, pretend that we
         have already sent the full message body. */
        conn->response_write_position = response->total_size;
    }

    ret = HTTPD_str_equal_caseless_(conn->method,
                                    HTTPD_HTTP_METHOD_POST);
    if ( (HTTPD_CONNECTION_HEADERS_PROCESSED == conn->state) &&
        (NULL != conn->method)) {
        ret = HTTPD_str_equal_caseless_(conn->method,
                                        HTTPD_HTTP_METHOD_POST);
        if (ret) {
            ret = HTTPD_str_equal_caseless_(conn->method,
                                            HTTPD_HTTP_METHOD_PUT);
            if (ret) {
                /* response was queued "early", refuse to read body / footers or
                 further requests! */
                conn->read_closed = 1;
                conn->state = HTTPD_CONNECTION_FOOTERS_RECEIVED;
            }
        }
    }

    if (!conn->in_idle)
        (void) httpd_connection_handle_idle (conn);
    return HTTPD_YES;
}

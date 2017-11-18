#include "macros.h"
#include "httpd.h"
#include "reason_phrase.h"

static const char *invalid_hundred[] = { NULL };

static const char *const one_hundred[] = {
    "Continue",
    "Switching Protocols",
    "Processing"
};

static const char *const two_hundred[] = {
    "OK",
    "Created",
    "Accepted",
    "Non-Authoritative Information",
    "No Content",
    "Reset Content",
    "Partial Content",
    "Multi Status"
};

static const char *const three_hundred[] = {
    "Multiple Choices",
    "Moved Permanently",
    "Moved Temporarily",
    "See Other",
    "Not Modified",
    "Use Proxy",
    "Switch Proxy",
    "Temporary Redirect"
};

static const char *const four_hundred[] = {
    "Bad Request",
    "Unauthorized",
    "Payment Required",
    "Forbidden",
    "Not Found",
    "Method Not Allowed",
    "Not Acceptable",
    "Proxy Authentication Required",
    "Request Time-out",
    "Conflict",
    "Gone",
    "Length Required",
    "Precondition Failed",
    "Request Entity Too Large",
    "Request-URI Too Large",
    "Unsupported Media Type",
    "Requested Range Not Satisfiable",
    "Expectation Failed",
    "Unknown",
    "Unknown",
    "Unknown", /* 420 */
    "Unknown",
    "Unprocessable Entity",
    "Locked",
    "Failed Dependency",
    "Unordered Collection",
    "Upgrade Required",
    "Unknown",
    "Unknown",
    "Unknown",
    "Unknown", /* 430 */
    "Unknown",
    "Unknown",
    "Unknown",
    "Unknown",
    "Unknown", /* 435 */
    "Unknown",
    "Unknown",
    "Unknown",
    "Unknown",
    "Unknown", /* 440 */
    "Unknown",
    "Unknown",
    "Unknown",
    "No Response",
    "Unknown", /* 445 */
    "Unknown",
    "Unknown",
    "Unknown",
    "Retry With",
    "Blocked by Windows Parental Controls", /* 450 */
    "Unavailable For Legal Reasons"
};

static const char *const five_hundred[] = {
    "Internal Server Error",
    "Not Implemented",
    "Bad Gateway",
    "Service Unavailable",
    "Gateway Time-out",
    "HTTP Version not supported",
    "Variant Also Negotiates",
    "Insufficient Storage",
    "Unknown",
    "Bandwidth Limit Exceeded",
    "Not Extended"
};

struct HTTPD_Reason_Block
{
    unsigned int max;
    const char *const*data;
};

#define BLOCK(m) { (sizeof(m) / sizeof(char*)), m }

static const struct HTTPD_Reason_Block reasons[] = {
    BLOCK (invalid_hundred),
    BLOCK (one_hundred),
    BLOCK (two_hundred),
    BLOCK (three_hundred),
    BLOCK (four_hundred),
    BLOCK (five_hundred),
};

const char *
HTTPD_get_reason_phrase_for (unsigned int code)
{
    if ( (code >= 100) &&
        (code < 600) &&
        (reasons[code / 100].max > (code % 100)) )
        return reasons[code / 100].data[code % 100];
    return "Unknown";
}
#include "macros.h"
#include "httpd_string.h"

#define isasciilower(c) (((char)(c)) >= 'a' && ((char)(c)) <= 'z')

#define isasciiupper(c) (((char)(c)) >= 'A' && ((char)(c)) <= 'Z')

#define toasciilower(c) ((isasciiupper(c)) ? (((char)(c)) - 'A' + 'a') : ((char)(c)))

#define toasciiupper(c) ((isasciilower(c)) ? (((char)(c)) - 'a' + 'A') : ((char)(c)))

#define isasciidigit(c) (((char)(c)) >= '0' && ((char)(c)) <= '9')

#define toxdigitvalue(c) ( isasciidigit(c) ? (int)(((char)(c)) - '0') : \
    ( (((char)(c)) >= 'A' && ((char)(c)) <= 'F') ? \
    (int)(((unsigned char)(c)) - 'A' + 10) : \
    ( (((char)(c)) >= 'a' && ((char)(c)) <= 'f') ? \
    (int)(((unsigned char)(c)) - 'a' + 10) : (int)(-1) )))


int HTTPD_str_equal_caseless_ (const char * str1, const char * str2)
{
    while (0 != (*str1))
    {
        const char c1 = *str1;
        const char c2 = *str2;
        if (c1 != c2 && toasciilower (c1) != toasciilower (c2))
            return 0;
        str1++;
        str2++;
    }
    return 0 == (*str2);
}

size_t HTTPD_str_to_uint64_ (const char * str, uint64_t * out_val)
{
    const char * const start = str;
    uint64_t res;
    if (!str || !out_val || !isasciidigit(str[0]))
        return 0;

    res = 0;
    do
    {
        const int digit = (unsigned char)(*str) - '0';
        if ( (res > (UINT64_MAX / 10)) ||
            (res == (UINT64_MAX / 10) && digit > (UINT64_MAX % 10)) )
            return 0;

        res *= 10;
        res += digit;
        str++;
    } while (isasciidigit (*str));

    *out_val = res;
    return str - start;
}

size_t HTTPD_strx_to_sizet_n_ (const char * str, size_t maxlen, size_t * out_val)
{
    size_t i;
    size_t res;
    int digit;
    if (!str || !out_val)
        return 0;

    res = 0;
    i = 0;
    digit = toxdigitvalue(str[i]);
    while (i < maxlen && digit >= 0)
    {
        if ( (res > (SIZE_MAX / 16)) ||
            (res == (SIZE_MAX / 16) && digit > (SIZE_MAX % 16)) )
            return 0;

        res *= 16;
        res += digit;
        i++;
    }

    if (i)
        *out_val = res;
    return i;
}

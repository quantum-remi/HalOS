#include "string.h"
#include "serial.h"

void *memset(void *dst, int c, size_t n)
{
    uint8_t *temp = dst;
    for (; n != 0; n--)
        *temp++ = c;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    uint8_t *ret = dst;
    uint8_t *p = dst;
    const uint8_t *q = src;
    while (n--)
        *p++ = *q++;
    return ret;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const uint8_t *p1 = s1, *p2 = s2;
    while (n--)
    {
        if (*p1 != *p2)
            return *p1 - *p2;
        p1++;
        p2++;
    }
    return 0;
}

size_t strlen(const char *s)
{
    size_t len = 0;
    while (*s++)
        len++;
    return len;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    size_t i = 0;

    // Copy characters from src to dest until n characters are copied or the end of src is reached.
    while (i < n && src[i] != '\0')
    {
        dest[i] = src[i];
        i++;
    }

    // Pad the remaining space in dest with null terminators.
    while (i < n)
    {
        dest[i++] = '\0';
    }

    return dest;
}

int strcmp(const char *s1, const char *s2)
{
    int i = 0;

    while ((s1[i] == s2[i]))
    {
        if (s2[i++] == 0)
            return 0;
    }
    return 1;
}

int strncmp(const char *s1, const char *s2, size_t c)
{
    int result = 0;

    while (c)
    {
        result = *s1 - *s2++;
        if ((result != 0) || (*s1++ == 0))
        {
            break;
        }
        c--;
    }
    return result;
}

char *strcpy(char *dst, const char *src)
{
    char *ret = dst;
    while ((*dst++ = *src++))
        ;
    return ret;
}

char *strcat(char *dest, const char *src)
{
    char *ret = dest;
    while (*dest)
        dest++;
    while ((*dest++ = *src++))
        ;
    return ret;
}

int isspace(int c)
{
    return (c == ' ' || (c >= '\t' && c <= '\r'));
}

int isalpha(int c)
{
    return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
}

char upper(char c)
{
    if ((c >= 'a') && (c <= 'z'))
        return (c - 32);
    return c;
}

char lower(char c)
{
    if ((c >= 'A') && (c <= 'Z'))
        return (c + 32);
    return c;
}

void itoa(char *buf, int base, int d)
{
    char *p = buf;
    char *p1, *p2;
    unsigned int ud = d;
    int divisor = 10;

    if (base == 'd' && d < 0)
    {
        *p++ = '-';
        buf++;
        ud = -d;
    }
    else if (base == 'x')
        divisor = 16;

    do
    {
        int remainder = ud % divisor;
        *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    } while (ud /= divisor);

    *p = 0;

    p1 = buf;
    p2 = p - 1;
    while (p1 < p2)
    {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
}
char *strstr(const char *in, const char *str)
{
    char c;
    size_t len;

    c = *str++;
    if (!c)
        return (char *)in;

    len = strlen(str);
    do
    {
        char sc;
        do
        {
            sc = *in++;
            if (!sc)
                return (char *)0;
        } while (sc != c);
    } while (strncmp(in, str, len) != 0);

    return (char *)(in - 1);
}

char *strchr(const char *s, int c)
{
    const char ch = (char)c;

    // Search through the string
    while (*s != '\0')
    {
        if (*s == ch)
        {
            return (char *)s; // Found match
        }
        s++;
    }

    // Check if we're looking for null terminator
    if (ch == '\0')
    {
        return (char *)s;
    }

    return NULL; // Character not found
}

void *memmove(void *dest, const void *src, size_t n)
{
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (pdest < psrc)
    {
        for (size_t i = 0; i < n; i++)
        {
            pdest[i] = psrc[i];
        }
    }
    else
    {
        for (size_t i = n; i > 0; i--)
        {
            pdest[i - 1] = psrc[i - 1];
        }
    }
    return dest;
}

int atoi(const char *str)
{
    int result = 0;
    int sign = 1;
    int i = 0;

    // Skip whitespace
    while (str[i] == ' ' || str[i] == '\t' || str[i] == '\n')
    {
        i++;
    }

    // Check for sign
    if (str[i] == '-')
    {
        sign = -1;
        i++;
    }
    else if (str[i] == '+')
    {
        i++;
    }

    // Convert digits to integer
    while (str[i] >= '0' && str[i] <= '9')
    {
        result = result * 10 + (str[i] - '0');
        i++;
    }

    return sign * result;
}

char *strncat(char *dest, const char *src, size_t n)
{
    size_t dest_len = strlen(dest);
    size_t i;

    for (i = 0; i < n && src[i] != '\0'; i++)
    {
        dest[dest_len + i] = src[i];
    }

    dest[dest_len + i] = '\0';

    return dest;
}

void assert(int condition)
{
    if (!condition)
    {
        serial_printf("Assertion failed!\n");
        while (1)
            ;
    }
}

const char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    while (*s)
    {
        if (*s == c)
            last = s;
        s++;
    }
    return last;
}

static char *last;

char *strtok(char *str, const char *delim)
{
    char *token_start;

    if (str)
        last = str;
    else if (!last)
        return NULL;

    // Skip leading delimiters
    while (*last && strchr(delim, *last))
        last++;

    if (!*last)
        return NULL;

    token_start = last;

    // Find end of token
    while (*last && !strchr(delim, *last))
        last++;

    if (*last)
        *last++ = '\0';
    else
        last = NULL;

    return token_start;
}

int toupper(int c)
{
    return (c >= 'a' && c <= 'z') ? (c & ~32) : c;
}

int tolower(int c)
{
    return (c >= 'A' && c <= 'Z') ? (c | 32) : c;
}
#include <string.h>
#include "u16str.h"

char16_t *u16memchr(const char16_t *s, char16_t c, size_t n)
{
    while (n >= 4)
    {
        if (s[0] == c)
            return (char16_t *)s;
        if (s[1] == c)
            return (char16_t *)&s[1];
        if (s[2] == c)
            return (char16_t *)&s[2];
        if (s[3] == c)
            return (char16_t *)&s[3];
        s += 4;
        n -= 4;
    }

    if (n > 0)
    {
        if (*s == c)
            return (char16_t *)s;
        ++s;
        --n;
    }
    if (n > 0)
    {
        if (*s == c)
            return (char16_t *)s;
        ++s;
        --n;
    }
    if (n > 0)
        if (*s == c)
            return (char16_t *)s;

    return NULL;
}

char16_t *u16memcpy(char16_t *s1, const char16_t *s2, size_t n)
{
    return (char16_t *)memcpy((char *)s1, (char *)s2, n * sizeof(char16_t));
}

char16_t *u16memset(char16_t *s, char16_t c, size_t n)
{
    char16_t *wp = s;

    while (n >= 4)
    {
        wp[0] = c;
        wp[1] = c;
        wp[2] = c;
        wp[3] = c;
        wp += 4;
        n -= 4;
    }

    if (n > 0)
    {
        wp[0] = c;

        if (n > 1)
        {
            wp[1] = c;

            if (n > 2)
                wp[2] = c;
        }
    }

    return s;
}

size_t u16len(const char16_t *s)
{
    size_t len = 0;

    while (s[len] != u'\0')
    {
        if (s[++len] == u'\0')
            return len;
        if (s[++len] == u'\0')
            return len;
        if (s[++len] == u'\0')
            return len;
        ++len;
    }

    return len;
}

size_t u16nlen(const char16_t *s, size_t maxlen)
{
    const char16_t *ret = u16memchr(s, L'\0', maxlen);
    if (ret)
        maxlen = ret - s;
    return maxlen;
}

char16_t *u16cpy(char16_t *dest, const char16_t *src)
{
    return u16memcpy(dest, src, u16len(src) + 1);
}

char16_t *u16ncpy(char16_t *dest, const char16_t *src, size_t n)
{
    size_t size = u16nlen(src, n);
    if (size != n)
        u16memset(dest + size, L'\0', n - size);
    return u16memcpy(dest, src, size);
}

int u16cmp(const char16_t *s1, const char16_t *s2)
{
    char16_t c1, c2;

    do
    {
        c1 = *s1++;
        c2 = *s2++;
        if (c2 == u'\0')
            return c1 - c2;
    } while (c1 == c2);

    return c1 < c2 ? -1 : 1;
}
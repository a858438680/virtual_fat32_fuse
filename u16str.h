#ifndef U16STR_H
#define U16STR_H
#include <stdlib.h>

std::string wide2local(std::u16string_view str);

std::u16string local2wide(std::string_view str);

char16_t *u16memchr(const char16_t *s, char16_t c, size_t n);

char16_t *u16memcpy(char16_t *s1, const char16_t *s2, size_t n);

char16_t *u16memset(char16_t *s, char16_t c, size_t n);

size_t u16len(const char16_t *s);

size_t u16nlen(const char16_t *s, size_t maxlen);

char16_t *u16cpy(char16_t *dest, const char16_t *src);

char16_t *u16ncpy(char16_t *dest, const char16_t *src, size_t n);

int u16cmp(const char16_t *s1, const char16_t *s2);

#endif
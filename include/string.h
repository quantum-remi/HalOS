#ifndef STRING_H
#define STRING_H

#include <stdint.h>
#include <stddef.h>

void *memset(void *dst, int c, size_t n);

void *memcpy(void *dst, const void *src, size_t n);

int memcmp(const void *s1, const void *s2, size_t n);

size_t strlen(const char *s);

char *strncpy(char *dest, const char *src, size_t n);

int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);

char *strcpy(char *dst, const char *src);

char *strcat(char *dest, const char *src);

int isspace(int c);

int isalpha(int c);
char upper(char c);
char lower(char c);

void itoa(char *buf, int base, int d);

char *strstr(const char *in, const char *str);

char *strchr(const char *s, int c);

void *memmove(void *dest, const void *src, size_t n);

int atoi(const char *str);

void assert(int condition);

char* strncat(char *dest, const char *src, size_t n);

const char *strrchr(const char *s, int c);

char *strtok(char *str, const char *delim);

int toupper(int c);

int tolower(int c);

int MIN(int a, int b);

#endif

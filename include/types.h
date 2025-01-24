#ifndef TYPES_H
#define TYPES_H

#define NULL 0

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef signed char sint8;
typedef signed short sint16;
typedef signed int sint32;
typedef unsigned long long uint64;
typedef uint8 byte;
typedef uint16 word;
typedef uint32 dword;

#if __SIZEOF_POINTER__ == 4
    typedef uint32 uintptr;
#elif __SIZEOF_POINTER__ == 8
    typedef uint64 uintptr;
#else
    #error "Unsupported pointer size"
#endif

typedef enum {
    FALSE,
    TRUE
} BOOL;

#endif
#ifndef LIBALLOC_HOOK_H
#define LIBALLOC_HOOK_H

#define BLOCK_SIZE 4096

#include <stdint.h>
#include <stddef.h>
#include "string.h"
#include "pmm.h"

int liballoc_lock();
int liballoc_unlock();
void* liballoc_alloc(int);
int liballoc_free(void*, int);

#endif
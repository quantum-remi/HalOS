#ifndef LIBALLOC_HOOK_H
#define LIBALLOC_HOOK_H

#include <stdint.h>
#include <stddef.h>
#include "string.h"
#include "vmm.h"
#include "paging.h"

#define BLOCK_SIZE PAGE_SIZE

int liballoc_lock();
int liballoc_unlock();
void *liballoc_alloc(int);
int liballoc_free(void *, int);

#endif
#ifndef LIBALLOC_HOOK_H
#define LIBALLOC_HOOK_H

#include "types.h"
#include "string.h"
#include "vmm.h"

int liballoc_lock();
int liballoc_unlock();
void* liballoc_alloc(int);
int liballoc_free(void*, int);

#endif
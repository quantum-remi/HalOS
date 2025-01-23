#ifndef VMM_H
#define VMM_H

#include "types.h"

void vmm_init();
void vmm_map_page(void *phys, void *virt);
void *vmm_alloc_blocks(int num_blocks);
void vmm_free_blocks(void *ptr, int num_blocks);

#endif

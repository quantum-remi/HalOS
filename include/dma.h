// dma.h
#pragma once
#include <stdint.h>
#include <stddef.h>

void* dma_alloc(size_t size, uint32_t* phys_addr);
void dma_free(void* virt, size_t size);
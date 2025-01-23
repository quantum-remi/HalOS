#ifndef PAGING_H
#define PAGING_H

#include "types.h"

typedef struct {
    uint32 present : 1;
    uint32 rw : 1;
    uint32 user : 1;
    uint32 write_through : 1;
    uint32 cache_disabled : 1;
    uint32 accessed : 1;
    uint32 dirty : 1;
    uint32 pat : 1;
    uint32 global : 1;
    uint32 available : 3;
    uint32 frame : 20;
} page_t;

typedef struct {
    page_t pages[1024];
} page_table_t;

typedef struct {
    page_table_t *tables[1024];
} page_directory_t;

void paging_init();
void load_page_directory(page_directory_t *dir);
void enable_paging();

page_directory_t *get_page_directory();

#endif

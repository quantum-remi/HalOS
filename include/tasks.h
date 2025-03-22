#ifndef TASKS_H
#define TASKS_H

#include <stdint.h>

void switch_to_userspace(uint32_t eip, uint32_t esp);

#endif
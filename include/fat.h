#ifndef FAT_H
#define FAT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>


struct DirectoryEntry
{
  char name[8];
  char ext[3];
  uint8_t attrib;
  uint8_t userattrib;

  char undelete;
  uint16_t  createtime;
  uint16_t  createdate;
  uint16_t  accessdate;
  uint16_t  clusterhigh;

  uint16_t  modifiedtime;
  uint16_t  modifieddate;
  uint16_t  clusterlow;
  uint32_t  filesize;
} __attribute__ ((packed));



#endif // FAT_H

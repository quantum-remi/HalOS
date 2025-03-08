#include "ide.h"

#define STATUS_BSY 0x80
#define STATUS_RDY 0x40
#define STATUS_DRQ 0x08
#define STATUS_DF 0x20
#define STATUS_ERR 0x01

static void ATA_wait_BSY();
static void ATA_wait_DRQ();

void read_sectors_ATA_PIO(uint8_t *target_address, uint32_t LBA,
                          uint8_t sector_count)
{
  // wait if it's busy
  ATA_wait_BSY();
  // 0xE0 -> master, 0xF0 -> slave, 4 highest bits of LBA
  outportb(ATA_MASTER_BASE + ATA_REG_HDDEVSEL,
           ATA_MASTER | ((LBA >> 24) & 0xF));
  // Send the amount of sectors we want
  outportb(ATA_MASTER_BASE + ATA_REG_SECCOUNT0, sector_count);
  // Send LBA, 8 bits at a time!
  outportb(ATA_MASTER_BASE + ATA_REG_LBA0, (uint8_t)LBA);
  outportb(ATA_MASTER_BASE + ATA_REG_LBA1, (uint8_t)(LBA >> 8));
  outportb(ATA_MASTER_BASE + ATA_REG_LBA2, (uint8_t)(LBA >> 16));
  // Read already!
  outportb(ATA_MASTER_BASE + ATA_REG_COMMAND, 0x20);

  uint16_t *target = (uint16_t *)target_address;

  for (int j = 0; j < sector_count; j++)
  {
    ATA_wait_BSY();
    ATA_wait_DRQ();
    for (int i = 0; i < 256; i++)
      target[i] = inportw(0x1F0);
    target += 256;
  }
}

void write_sectors_ATA_PIO(uint32_t LBA, uint8_t sector_count,
                           uint8_t *rawBytes)
{
  ATA_wait_BSY();
  outportb(0x1F6, 0xE0 | ((LBA >> 24) & 0xF));
  outportb(0x1F2, sector_count);
  outportb(0x1F3, (uint8_t)LBA);
  outportb(0x1F4, (uint8_t)(LBA >> 8));
  outportb(0x1F5, (uint8_t)(LBA >> 16));
  outportb(0x1F7, 0x30); // Send the write command

  uint32_t *bytes = (uint32_t *)rawBytes;

  for (int j = 0; j < sector_count; j++)
  {
    ATA_wait_BSY();
    ATA_wait_DRQ();
    for (int i = 0; i < 256; i++)
    {
      outportl(0x1F0, bytes[i]);
    }
    bytes += 256;
  }
}

static void ATA_wait_BSY() // Wait for bsy to be 0
{
  while (inportb(0x1F7) & STATUS_BSY)
    ;
}
static void ATA_wait_DRQ() // Wait fot drq to be 1
{
  while (!(inportb(0x1F7) & STATUS_RDY))
    ;
}
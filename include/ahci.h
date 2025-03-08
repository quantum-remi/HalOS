#ifndef AHCI_H
#define AHCI_H

#include <stdint.h>
#include <stddef.h>

#include "pci.h"
#include "io.h"
#include "string.h"

#define PCI_CLASS_STORAGE 0x01
#define PCI_SUBCLASS_SATA 0x06
#define PCI_PROGIF_AHCI 0x01

// AHCI structure definitions
#define AHCI_MAX_PORTS 32
#define AHCI_CMD_SLOTS 32
#define AHCI_FIS_SIZE 256
#define AHCI_PRDT_ENTRIES 8

typedef struct [[gnu::packed]]
{
    uint32_t clb;
    uint32_t clbu;
    uint32_t fb;
    uint32_t fbu;
    uint32_t is;
    uint32_t ie;
    uint32_t cmd;
    uint32_t rsv0;
    uint32_t tfd;
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;
    uint32_t rsv1[11];
    uint32_t vendor[4];
} HBA_PORT;

// HBA Registers
typedef struct [[gnu::packed]]
{
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vsn;
    uint32_t ccc_ctl;
    uint32_t ccc_pts;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;
    HBA_PORT ports[AHCI_MAX_PORTS]; // Add ports array
} HBA_MEM;

// FIS Types
enum
{
    FIS_TYPE_REG_H2D = 0x27,
    FIS_TYPE_REG_D2H = 0x34,
    FIS_TYPE_DMA_ACT = 0x39,
    FIS_TYPE_DMA_SETUP = 0x41,
    FIS_TYPE_DATA = 0x46,
    FIS_TYPE_BIST = 0x58,
    FIS_TYPE_PIO_SETUP = 0x5F,
    FIS_TYPE_DEV_BITS = 0xA1,
};

// Command FIS
typedef struct [[gnu::packed]]
{
    uint8_t fis_type;
    uint8_t pm_port : 4;
    uint8_t rsv0 : 3;
    uint8_t c : 1;
    uint8_t command;
    uint8_t featurel;

    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;

    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t featureh;

    uint8_t countl;
    uint8_t counth;
    uint8_t icc;
    uint8_t control;

    uint8_t rsv1[4];
} FIS_REG_H2D;

// Command List
typedef struct [[gnu::packed]]
{
    uint8_t cfis[64];  // Command FIS
    uint8_t atapi[16]; // ATAPI command
    uint32_t prdtl;    // PRDT length
    uint32_t prdbc;    // PRDT byte count
    uint32_t ctba;     // Command table base
    uint32_t ctbau;    // Command table base upper
    uint32_t rsv[4];
} HBA_CMD_HEADER;

// PRDT Entry
typedef struct [[gnu::packed]]
{
    uint32_t dba;  // Data base address
    uint32_t dbau; // Data base address upper
    uint32_t rsv0;
    uint32_t dbc : 22; // Byte count
    uint32_t rsv1 : 9;
    uint32_t i : 1;
} HBA_PRDT_ENTRY;

// Command Table
typedef struct [[gnu::packed]]
{
    uint8_t cfis[64];                       // Command FIS
    uint8_t acmd[16];                       // ATAPI command
    uint8_t rsv[48];                        // Reserved
    HBA_PRDT_ENTRY prdt[AHCI_PRDT_ENTRIES]; // PRDT
} HBA_CMD_TABLE;

// AHCI Port Structure
typedef struct
{
    HBA_PORT *regs;
    uint8_t *cl;       // Command list
    uint8_t *fis;      // FIS receive area
    HBA_CMD_TABLE *ct; // Command tables
    uint32_t cl_phys;  // Physical address of CL
    uint32_t fis_phys; // Physical address of FIS
    uint32_t ct_phys;  // Physical address of CT
    uint8_t slot;
} AHCI_PORT;

// Driver structure
typedef struct
{
    HBA_MEM *mem;
    AHCI_PORT ports[AHCI_MAX_PORTS];
    uint32_t num_ports;
} AHCI_DRIVER;

// Function prototypes
int ahci_init(AHCI_DRIVER *drv);
int ahci_port_init(AHCI_DRIVER *drv, int port);
int ahci_read(AHCI_DRIVER *drv, int port, uint64_t lba, void *buffer, size_t count);

// TODO
void *dma_alloc(size_t size, uint32_t *phys_addr);
void dma_free(void *virt, size_t size);

#endif // AHCI_H
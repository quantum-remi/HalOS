#include <stdint.h>
#include <stddef.h>

#include "ide.h"
#include "console.h"
#include "io.h"
#include "string.h"
#include "serial.h"

IDE_CHANNELS g_ide_channels[MAXIMUM_CHANNELS];
IDE_DEVICE g_ide_devices[MAXIMUM_IDE_DEVICES];

static volatile unsigned char g_ide_irq_invoked = 0;

static uint8_t ide_read_register(uint8_t channel, uint8_t reg);
static void ide_write_register(uint8_t channel, uint8_t reg, uint8_t data);

static uint8_t ide_read_register(uint8_t channel, uint8_t reg)
{
    uint8_t ret;
    if (reg > 0x07 && reg < 0x0C)
        ide_write_register(channel, ATA_REG_CONTROL, 0x80 | g_ide_channels[channel].no_intr);
    if (reg < 0x08)
        ret = inportb(g_ide_channels[channel].base + reg - 0x00);
    else if (reg < 0x0C)
        ret = inportb(g_ide_channels[channel].base + reg - 0x06);
    else if (reg < 0x0E)
        ret = inportb(g_ide_channels[channel].control + reg - 0x0A);
    else if (reg < 0x16)
        ret = inportb(g_ide_channels[channel].bm_ide + reg - 0x0E);
    if (reg > 0x07 && reg < 0x0C)
        ide_write_register(channel, ATA_REG_CONTROL, g_ide_channels[channel].no_intr);
    return ret;
}

static void ide_write_register(uint8_t channel, uint8_t reg, uint8_t data)
{
    if (reg > 0x07 && reg < 0x0C)
        ide_write_register(channel, ATA_REG_CONTROL, 0x80 | g_ide_channels[channel].no_intr);
    if (reg < 0x08)
        outportb(g_ide_channels[channel].base + reg - 0x00, data);
    else if (reg < 0x0C)
        outportb(g_ide_channels[channel].base + reg - 0x06, data);
    else if (reg < 0x0E)
        outportb(g_ide_channels[channel].control + reg - 0x0A, data);
    else if (reg < 0x16)
        outportb(g_ide_channels[channel].bm_ide + reg - 0x0E, data);
    if (reg > 0x07 && reg < 0x0C)
        ide_write_register(channel, ATA_REG_CONTROL, g_ide_channels[channel].no_intr);
}

void insl(uint16_t reg, uint32_t *buffer, size_t quads)
{
    size_t index;
    for (index = 0; index < quads; index++)
    {
        buffer[index] = inportl(reg);
    }
}

void outsl(uint16_t reg, uint32_t *buffer, size_t quads)
{
    size_t index;
    for (index = 0; index < quads; index++)
    {
        outportl(reg, buffer[index]);
    }
}

void ide_read_buffer(uint8_t channel, uint8_t reg, uint32_t *buffer, size_t quads)
{
    if (reg > 0x07 && reg < 0x0C)
        ide_write_register(channel, ATA_REG_CONTROL, 0x80 | g_ide_channels[channel].no_intr);
    __asm__("pushw %es");
    __asm__("movw %ds, %ax");
    __asm__("movw %ax, %es");
    if (reg < 0x08)
        insl(g_ide_channels[channel].base + reg - 0x00, buffer, quads);
    else if (reg < 0x0C)
        insl(g_ide_channels[channel].base + reg - 0x06, buffer, quads);
    else if (reg < 0x0E)
        insl(g_ide_channels[channel].control + reg - 0x0A, buffer, quads);
    else if (reg < 0x16)
        insl(g_ide_channels[channel].bm_ide + reg - 0x0E, buffer, quads);
    __asm__("popw %es;");
    if (reg > 0x07 && reg < 0x0C)
        ide_write_register(channel, ATA_REG_CONTROL, g_ide_channels[channel].no_intr);
}

void ide_write_buffer(uint8_t channel, uint8_t reg, uint32_t *buffer, uint32_t quads)
{
    if (reg > 0x07 && reg < 0x0C)
        ide_write_register(channel, ATA_REG_CONTROL, 0x80 | g_ide_channels[channel].no_intr);
    __asm__("pushw %es");
    __asm__("movw %ds, %ax");
    __asm__("movw %ax, %es");
    if (reg < 0x08)
        outsl(g_ide_channels[channel].base + reg - 0x00, buffer, quads);
    else if (reg < 0x0C)
        outsl(g_ide_channels[channel].base + reg - 0x06, buffer, quads);
    else if (reg < 0x0E)
        outsl(g_ide_channels[channel].control + reg - 0x0A, buffer, quads);
    else if (reg < 0x16)
        outsl(g_ide_channels[channel].bm_ide + reg - 0x0E, buffer, quads);
    __asm__("popw %es;");
    if (reg > 0x07 && reg < 0x0C)
        ide_write_register(channel, ATA_REG_CONTROL, g_ide_channels[channel].no_intr);
}

uint8_t ide_polling(uint8_t channel, uint8_t advanced_check)
{
    for (int i = 0; i < 4; i++)
        ide_read_register(channel, ATA_REG_ALTSTATUS);
    while (ide_read_register(channel, ATA_REG_STATUS) & ATA_SR_BSY)
        ;
    if (advanced_check)
    {
        uint8_t state = ide_read_register(channel, ATA_REG_STATUS);
        if (state & ATA_SR_ERR)
            return 2;
        if (state & ATA_SR_DF)
            return 1;
        if ((state & ATA_SR_DRQ) == 0)
            return 3;
    }
    return 0;
}

uint8_t ide_print_error(uint32_t drive, uint8_t err)
{
    if (err == 0)
        return err;
    console_printf("IDE:");
    if (err == 1)
    {
        console_printf("- Device Fault\n");
        err = 19;
    }
    else if (err == 2)
    {
        uint8_t st = ide_read_register(g_ide_devices[drive].channel, ATA_REG_ERROR);
        if (st & ATA_ER_AMNF)
        {
            console_printf("- No Address Mark Found\n");
            err = 7;
        }
        if (st & ATA_ER_TK0NF)
        {
            console_printf("- No Media or Media Error\n");
            err = 3;
        }
        if (st & ATA_ER_ABRT)
        {
            console_printf("- Command Aborted\n");
            err = 20;
        }
        if (st & ATA_ER_MCR)
        {
            console_printf("- No Media or Media Error\n");
            err = 3;
        }
        if (st & ATA_ER_IDNF)
        {
            console_printf("- ID mark not Found\n");
            err = 21;
        }
        if (st & ATA_ER_MC)
        {
            console_printf("- No Media or Media Error\n");
            err = 3;
        }
        if (st & ATA_ER_UNC)
        {
            console_printf("- Uncorrectable Data Error\n");
            err = 22;
        }
        if (st & ATA_ER_BBK)
        {
            console_printf("- Bad Sectors\n");
            err = 13;
        }
    }
    else if (err == 3)
    {
        console_printf("- Reads Nothing\n");
        err = 23;
    }
    else if (err == 4)
    {
        console_printf("- Write Protected\n");
        err = 8;
    }
    console_printf("- [%s %s] %s\n",
                   (const char *[]){"Primary", "Secondary"}[g_ide_devices[drive].channel],
                   (const char *[]){"Master", "Slave"}[g_ide_devices[drive].drive],
                   g_ide_devices[drive].model);
    return err;
}

void ide_init(uint32_t prim_channel_base_addr, uint32_t prim_channel_control_base_addr,
              uint32_t sec_channel_base_addr, uint32_t sec_channel_control_addr,
              uint32_t bus_master_addr)
{
    int i, j, k, count = 0;
    unsigned char ide_buf[2048] = {0};
    g_ide_channels[ATA_PRIMARY].base = prim_channel_base_addr;
    g_ide_channels[ATA_PRIMARY].control = prim_channel_control_base_addr;
    g_ide_channels[ATA_SECONDARY].base = sec_channel_base_addr;
    g_ide_channels[ATA_SECONDARY].control = sec_channel_control_addr;
    g_ide_channels[ATA_PRIMARY].bm_ide = bus_master_addr;
    g_ide_channels[ATA_SECONDARY].bm_ide = bus_master_addr;
    ide_write_register(ATA_PRIMARY, ATA_REG_CONTROL, 2);
    ide_write_register(ATA_SECONDARY, ATA_REG_CONTROL, 2);
    for (i = 0; i < 2; i++)
    {
        for (j = 0; j < 2; j++)
        {
            uint8_t err = 0, type = IDE_ATA, status;
            g_ide_devices[count].reserved = 0;
            ide_write_register(i, ATA_REG_HDDEVSEL, 0xA0 | (j << 4));
            ide_write_register(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
            if (ide_read_register(i, ATA_REG_STATUS) == 0)
                continue;
            while (1)
            {
                status = ide_read_register(i, ATA_REG_STATUS);
                if ((status & ATA_SR_ERR))
                {
                    err = 1;
                    break;
                }
                if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ))
                    break;
            }
            if (err != 0)
            {
                unsigned char cl = ide_read_register(i, ATA_REG_LBA1);
                unsigned char ch = ide_read_register(i, ATA_REG_LBA2);
                if (cl == 0x14 && ch == 0xEB)
                    type = IDE_ATAPI;
                else if (cl == 0x69 && ch == 0x96)
                    type = IDE_ATAPI;
                else
                    continue;
                ide_write_register(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
            }
            ide_read_buffer(i, ATA_REG_DATA, (uint32_t *)ide_buf, 128);
            g_ide_devices[count].reserved = 1;
            g_ide_devices[count].type = type;
            g_ide_devices[count].channel = i;
            g_ide_devices[count].drive = j;
            g_ide_devices[count].signature = *((unsigned short *)(ide_buf + ATA_IDENT_DEVICETYPE));
            g_ide_devices[count].features = *((unsigned short *)(ide_buf + ATA_IDENT_CAPABILITIES));
            g_ide_devices[count].command_sets = *((unsigned int *)(ide_buf + ATA_IDENT_COMMANDSETS));
            if (g_ide_devices[count].command_sets & (1 << 26))
                g_ide_devices[count].size = *((unsigned int *)(ide_buf + ATA_IDENT_MAX_LBA_EXT));
            else
                g_ide_devices[count].size = *((unsigned int *)(ide_buf + ATA_IDENT_MAX_LBA));
            for (k = 0; k < 40; k += 2)
            {
                g_ide_devices[count].model[k] = ide_buf[ATA_IDENT_MODEL + k + 1];
                g_ide_devices[count].model[k + 1] = ide_buf[ATA_IDENT_MODEL + k];
            }
            g_ide_devices[count].model[40] = '\0';
            for (k = 39; k >= 0; k--)
            {
                char ch = g_ide_devices[count].model[k];
                if (ch == ' ')
                    g_ide_devices[count].model[k] = '\0';
                else
                    break;
            }
            count++;
        }
    }
}
uint8_t ide_ata_access(uint8_t direction, uint8_t drive, uint32_t lba, uint8_t num_sectors, uint32_t buffer)
{
    uint8_t lba_mode, dma, cmd;
    uint8_t lba_io[6];
    uint32_t channel = g_ide_devices[drive].channel;
    uint32_t slavebit = g_ide_devices[drive].drive;
    uint32_t bus = g_ide_channels[channel].base;
    uint32_t words = 256;
    uint16_t cyl, i;
    uint8_t head, sect, err;
    ide_write_register(channel, ATA_REG_CONTROL, g_ide_channels[channel].no_intr = (g_ide_irq_invoked = 0x0) + 0x02);
    if (lba >= 0x10000000)
    {
        lba_mode = LBA_MODE_48;
        lba_io[0] = (lba & 0x000000FF) >> 0;
        lba_io[1] = (lba & 0x0000FF00) >> 8;
        lba_io[2] = (lba & 0x00FF0000) >> 16;
        lba_io[3] = (lba & 0xFF000000) >> 24;
        lba_io[4] = 0;
        lba_io[5] = 0;
        head = 0;
    }
    else if (g_ide_devices[drive].features & 0x200)
    {
        lba_mode = LBA_MODE_28;
        lba_io[0] = (lba & 0x00000FF) >> 0;
        lba_io[1] = (lba & 0x000FF00) >> 8;
        lba_io[2] = (lba & 0x0FF0000) >> 16;
        lba_io[3] = 0;
        lba_io[4] = 0;
        lba_io[5] = 0;
        head = (lba & 0xF000000) >> 24;
    }
    else
    {
        lba_mode = LBA_MODE_CHS;
        sect = (lba % 63) + 1;
        cyl = (lba + 1 - sect) / (16 * 63);
        lba_io[0] = sect;
        lba_io[1] = (cyl >> 0) & 0xFF;
        lba_io[2] = (cyl >> 8) & 0xFF;
        lba_io[3] = 0;
        lba_io[4] = 0;
        lba_io[5] = 0;
        head = (lba + 1 - sect) % (16 * 63) / (63);
    }
    dma = 0;
    while (ide_read_register(channel, ATA_REG_STATUS) & ATA_SR_BSY)
    {
    }
    if (lba_mode == LBA_MODE_CHS)
        ide_write_register(channel, ATA_REG_HDDEVSEL, 0xA0 | (slavebit << 4) | head);
    else
        ide_write_register(channel, ATA_REG_HDDEVSEL, 0xE0 | (slavebit << 4) | head);
    if (lba_mode == LBA_MODE_48)
    {
        ide_write_register(channel, ATA_REG_SECCOUNT1, 0);
        ide_write_register(channel, ATA_REG_LBA3, lba_io[3]);
        ide_write_register(channel, ATA_REG_LBA4, lba_io[4]);
        ide_write_register(channel, ATA_REG_LBA5, lba_io[5]);
    }
    ide_write_register(channel, ATA_REG_SECCOUNT0, num_sectors);
    ide_write_register(channel, ATA_REG_LBA0, lba_io[0]);
    ide_write_register(channel, ATA_REG_LBA1, lba_io[1]);
    ide_write_register(channel, ATA_REG_LBA2, lba_io[2]);
    if (lba_mode == LBA_MODE_CHS && dma == 0 && direction == ATA_READ)
        cmd = ATA_CMD_READ_PIO;
    if (lba_mode == LBA_MODE_28 && dma == 0 && direction == ATA_READ)
        cmd = ATA_CMD_READ_PIO;
    if (lba_mode == LBA_MODE_48 && dma == 0 && direction == ATA_READ)
        cmd = ATA_CMD_READ_PIO_EXT;
    if (lba_mode == LBA_MODE_CHS && dma == 1 && direction == ATA_READ)
        cmd = ATA_CMD_READ_DMA;
    if (lba_mode == LBA_MODE_28 && dma == 1 && direction == ATA_READ)
        cmd = ATA_CMD_READ_DMA;
    if (lba_mode == LBA_MODE_48 && dma == 1 && direction == ATA_READ)
        cmd = ATA_CMD_READ_DMA_EXT;
    if (lba_mode == LBA_MODE_CHS && dma == 0 && direction == ATA_WRITE)
        cmd = ATA_CMD_WRITE_PIO;
    if (lba_mode == LBA_MODE_28 && dma == 0 && direction == ATA_WRITE)
        cmd = ATA_CMD_WRITE_PIO;
    if (lba_mode == LBA_MODE_48 && dma == 0 && direction == ATA_WRITE)
        cmd = ATA_CMD_WRITE_PIO_EXT;
    if (lba_mode == LBA_MODE_CHS && dma == 1 && direction == ATA_WRITE)
        cmd = ATA_CMD_WRITE_DMA;
    if (lba_mode == LBA_MODE_28 && dma == 1 && direction == ATA_WRITE)
        cmd = ATA_CMD_WRITE_DMA;
    if (lba_mode == LBA_MODE_48 && dma == 1 && direction == ATA_WRITE)
        cmd = ATA_CMD_WRITE_DMA_EXT;
    ide_write_register(channel, ATA_REG_COMMAND, cmd);
    if (dma)
    {
        if (direction == ATA_READ)
        {
        }
        else
        {
        }
    }
    else if (direction == ATA_READ)
    {
        for (i = 0; i < num_sectors; i++)
        {
            if ((err = ide_polling(channel, 1)))
                return err;
            __asm__("pushw %es");
            __asm__("rep insw"
                    :
                    : "c"(words), "d"(bus), "D"(buffer));
            __asm__("popw %es");
            buffer += (words * 2);
        }
    }
    else
    {
        for (i = 0; i < num_sectors; i++)
        {
            ide_polling(channel, 0);
            __asm__("pushw %ds");
            __asm__("rep outsw" ::"c"(words), "d"(bus), "S"(buffer));
            __asm__("popw %ds");
            buffer += (words * 2);
        }
        ide_write_register(channel, ATA_REG_COMMAND, (char[]){ATA_CMD_CACHE_FLUSH, ATA_CMD_CACHE_FLUSH, ATA_CMD_CACHE_FLUSH_EXT}[lba_mode]);
        ide_polling(channel, 0);
    }
    return 0;
}

void ide_wait_irq()
{
    while (!g_ide_irq_invoked)
        ;
    g_ide_irq_invoked = 0;
}
void ide_irq()
{
    g_ide_irq_invoked = 1;
}

int ide_read_sectors(uint8_t drive, uint8_t num_sectors, uint32_t lba, uint32_t buffer)
{
    if (drive > MAXIMUM_IDE_DEVICES || g_ide_devices[drive].reserved == 0)
    {
        console_printf("IDE ERROR: Drive not found\n");
        return -1;
    }
    else if (((lba + num_sectors) > g_ide_devices[drive].size) && (g_ide_devices[drive].type == IDE_ATA))
    {
        console_printf("IDE ERROR: LBA address(0x%x) is greater than the available drive sectors(0x%x)\n", lba, g_ide_devices[drive].size);
        return -2;
    }
    else
    {
        uint8_t err;
        if (g_ide_devices[drive].type == IDE_ATA)
            err = ide_ata_access(ATA_READ, drive, lba, num_sectors, buffer);
        return ide_print_error(drive, err);
    }
    return 0;
}

int ide_write_sectors(uint8_t drive, uint8_t num_sectors, uint32_t lba, uint32_t buffer)
{
    if (drive > MAXIMUM_IDE_DEVICES || g_ide_devices[drive].reserved == 0)
    {
        console_printf("IDE ERROR: Drive not found\n");
        return -1;
    }
    else if (((lba + num_sectors) > g_ide_devices[drive].size) && (g_ide_devices[drive].type == IDE_ATA))
    {
        console_printf("IDE ERROR: LBA address(0x%x) is greater than the available drive sectors(0x%x)\n", lba, g_ide_devices[drive].size);
        return -2;
    }
    else
    {
        uint8_t err;
        if (g_ide_devices[drive].type == IDE_ATA)
            err = ide_ata_access(ATA_WRITE, drive, lba, num_sectors, buffer);
        return ide_print_error(drive, err);
    }
    return 0;
}

void ata_init()
{
    ide_init(0x1F0, 0x3F6, 0x170, 0x376, 0x000);
}

int ata_get_drive_by_model(const char *model)
{
    int i;
    for (i = 0; i < MAXIMUM_IDE_DEVICES; i++)
    {
        if (strcmp((const char *)g_ide_devices[i].model, (char *)model) == 0)
            return i;
    }
    return -1;
}

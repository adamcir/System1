#include "types.h"
#include "fs_core.h"
#include "floppy_controller.h"

/* PC 82077-compatible FDC, 8237 DMA channel 2. Built in for floppy boot.
 * Disk modules cannot be loaded from floppy until this driver is available.
 */
static uint8_t pc_fdc_ready;

static uint8_t pc_fdc_inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void pc_fdc_outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void pc_fdc_io_delay(void) {
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}


#define FDC_DOR 0x3F2u
#define FDC_MSR 0x3F4u
#define FDC_FIFO 0x3F5u
#define FDC_CCR 0x3F7u
#define FDC_SECTORS_PER_TRACK 18u
#define FDC_HEADS 2u

static int pc_fdc_wait_send(void) {
    uint32_t i;

    for (i = 0u; i < 1000000u; ++i) {
        uint8_t msr = pc_fdc_inb(FDC_MSR);
        if ((msr & 0x80u) != 0u && (msr & 0x40u) == 0u) {
            return FS_OK;
        }
    }

    return FS_ERR_READ_ONLY;
}

static int pc_fdc_wait_recv(void) {
    uint32_t i;

    for (i = 0u; i < 1000000u; ++i) {
        uint8_t msr = pc_fdc_inb(FDC_MSR);
        if ((msr & 0x80u) != 0u && (msr & 0x40u) != 0u) {
            return FS_OK;
        }
    }

    return FS_ERR_READ_ONLY;
}

static int pc_fdc_send(uint8_t value) {
    int rc = pc_fdc_wait_send();
    if (rc != FS_OK) {
        return rc;
    }
    pc_fdc_outb(FDC_FIFO, value);
    return FS_OK;
}

static int pc_fdc_recv(uint8_t* value) {
    int rc;

    if (value == 0) {
        return FS_ERR_INVALID;
    }

    rc = pc_fdc_wait_recv();
    if (rc != FS_OK) {
        return rc;
    }

    *value = pc_fdc_inb(FDC_FIFO);
    return FS_OK;
}

static int pc_fdc_sense_interrupt(uint8_t* st0, uint8_t* cyl) {
    int rc;

    rc = pc_fdc_send(0x08u);
    if (rc != FS_OK) {
        return rc;
    }
    rc = pc_fdc_recv(st0);
    if (rc != FS_OK) {
        return rc;
    }
    return pc_fdc_recv(cyl);
}

static int pc_fdc_reset(void) {
    uint32_t i;
    uint8_t st0 = 0u;
    uint8_t cyl = 0u;
    int rc;

    pc_fdc_outb(FDC_DOR, 0x00u);
    for (i = 0u; i < 10000u; ++i) {
        pc_fdc_io_delay();
    }
    pc_fdc_outb(FDC_DOR, 0x1Cu);
    pc_fdc_outb(FDC_CCR, 0x00u);
    for (i = 0u; i < 10000u; ++i) {
        pc_fdc_io_delay();
    }

    for (i = 0u; i < 4u; ++i) {
        (void)pc_fdc_sense_interrupt(&st0, &cyl);
    }

    rc = pc_fdc_send(0x03u);
    if (rc != FS_OK) {
        return rc;
    }
    rc = pc_fdc_send(0xDFu);
    if (rc != FS_OK) {
        return rc;
    }
    return pc_fdc_send(0x02u);
}

static int pc_fdc_recalibrate(void) {
    uint32_t i;
    int rc;
    uint8_t st0 = 0u;
    uint8_t cyl = 0u;

    rc = pc_fdc_send(0x07u);
    if (rc != FS_OK) {
        return rc;
    }
    rc = pc_fdc_send(0x00u);
    if (rc != FS_OK) {
        return rc;
    }

    for (i = 0u; i < 100000u; ++i) {
        rc = pc_fdc_sense_interrupt(&st0, &cyl);
        if (rc == FS_OK && (st0 & 0x20u) != 0u) {
            return (cyl == 0u) ? FS_OK : FS_ERR_READ_ONLY;
        }
    }

    return FS_ERR_READ_ONLY;
}

static int pc_fdc_prepare(void) {
    int rc;

    if (pc_fdc_ready != 0u) {
        return FS_OK;
    }

    rc = pc_fdc_reset();
    if (rc != FS_OK) {
        return rc;
    }
    rc = pc_fdc_recalibrate();
    if (rc != FS_OK) {
        return rc;
    }

    pc_fdc_ready = 1u;
    return FS_OK;
}

static int pc_fdc_seek(uint8_t cylinder, uint8_t head) {
    uint32_t i;
    int rc;
    uint8_t st0 = 0u;
    uint8_t cyl = 0u;

    rc = pc_fdc_send(0x0Fu);
    if (rc != FS_OK) {
        return rc;
    }
    rc = pc_fdc_send((uint8_t)((head << 2) | 0u));
    if (rc != FS_OK) {
        return rc;
    }
    rc = pc_fdc_send(cylinder);
    if (rc != FS_OK) {
        return rc;
    }

    for (i = 0u; i < 100000u; ++i) {
        rc = pc_fdc_sense_interrupt(&st0, &cyl);
        if (rc == FS_OK && (st0 & 0x20u) != 0u) {
            return (cyl == cylinder) ? FS_OK : FS_ERR_READ_ONLY;
        }
    }

    return FS_ERR_READ_ONLY;
}

static void pc_dma2_setup_write(uint32_t addr, uint16_t count) {
    pc_fdc_outb(0x0Au, 0x06u);
    pc_fdc_outb(0x0Cu, 0xFFu);
    pc_fdc_outb(0x04u, (uint8_t)(addr & 0xFFu));
    pc_fdc_outb(0x04u, (uint8_t)((addr >> 8) & 0xFFu));
    pc_fdc_outb(0x81u, (uint8_t)((addr >> 16) & 0xFFu));
    pc_fdc_outb(0x0Cu, 0xFFu);
    pc_fdc_outb(0x05u, (uint8_t)(count & 0xFFu));
    pc_fdc_outb(0x05u, (uint8_t)((count >> 8) & 0xFFu));
    pc_fdc_outb(0x0Bu, 0x4Au);
    pc_fdc_outb(0x0Au, 0x02u);
}

static void pc_dma2_setup_read(uint32_t addr, uint16_t count) {
    pc_fdc_outb(0x0Au, 0x06u);
    pc_fdc_outb(0x0Cu, 0xFFu);
    pc_fdc_outb(0x04u, (uint8_t)(addr & 0xFFu));
    pc_fdc_outb(0x04u, (uint8_t)((addr >> 8) & 0xFFu));
    pc_fdc_outb(0x81u, (uint8_t)((addr >> 16) & 0xFFu));
    pc_fdc_outb(0x0Cu, 0xFFu);
    pc_fdc_outb(0x05u, (uint8_t)(count & 0xFFu));
    pc_fdc_outb(0x05u, (uint8_t)((count >> 8) & 0xFFu));
    pc_fdc_outb(0x0Bu, 0x46u);
    pc_fdc_outb(0x0Au, 0x02u);
}

static int pc_fdc_transfer_sector(void* sector_buffer, uint32_t lba, uint8_t write) {
    uint32_t track_size = FDC_SECTORS_PER_TRACK * FDC_HEADS;
    uint8_t cylinder = (uint8_t)(lba / track_size);
    uint8_t temp = (uint8_t)(lba % track_size);
    uint8_t head = (uint8_t)(temp / FDC_SECTORS_PER_TRACK);
    uint8_t sector = (uint8_t)((temp % FDC_SECTORS_PER_TRACK) + 1u);
    uint32_t addr = (uint32_t)(uintptr_t)sector_buffer;
    uint8_t result[7];
    uint32_t i;
    int rc;

    if (sector_buffer == 0) {
        return FS_ERR_INVALID;
    }

    rc = pc_fdc_prepare();
    if (rc != FS_OK) {
        return rc;
    }

    if (((addr & 0xFFFFu) + 511u) > 0xFFFFu) {
        return FS_ERR_INVALID;
    }

    rc = pc_fdc_seek(cylinder, head);
    if (rc != FS_OK) {
        return rc;
    }

    if (write != 0u) {
        pc_dma2_setup_write(addr, 511u);
    } else {
        pc_dma2_setup_read(addr, 511u);
    }

    rc = pc_fdc_send((write != 0u) ? 0x45u : 0x46u);
    if (rc != FS_OK) {
        return rc;
    }
    rc = pc_fdc_send((uint8_t)((head << 2) | 0u));
    if (rc != FS_OK) {
        return rc;
    }
    rc = pc_fdc_send(cylinder);
    if (rc != FS_OK) {
        return rc;
    }
    rc = pc_fdc_send(head);
    if (rc != FS_OK) {
        return rc;
    }
    rc = pc_fdc_send(sector);
    if (rc != FS_OK) {
        return rc;
    }
    rc = pc_fdc_send(0x02u);
    if (rc != FS_OK) {
        return rc;
    }
    rc = pc_fdc_send(FDC_SECTORS_PER_TRACK);
    if (rc != FS_OK) {
        return rc;
    }
    rc = pc_fdc_send(0x1Bu);
    if (rc != FS_OK) {
        return rc;
    }
    rc = pc_fdc_send(0xFFu);
    if (rc != FS_OK) {
        return rc;
    }

    for (i = 0u; i < 7u; ++i) {
        rc = pc_fdc_recv(&result[i]);
        if (rc != FS_OK) {
            return rc;
        }
    }

    if ((result[0] & 0xC0u) != 0u || result[1] != 0u || result[2] != 0u) {
        return FS_ERR_READ_ONLY;
    }

    return FS_OK;
}

static int pc_fdc_read_sector_buffer(void* sector_buffer, uint32_t lba) {
    return pc_fdc_transfer_sector(sector_buffer, lba, 0u);
}

static int pc_fdc_write_sector_buffer(const void* sector_buffer, uint32_t lba) {
    return pc_fdc_transfer_sector((void*)sector_buffer, lba, 1u);
}


static void pc_fdc_reset_state(void) {
    pc_fdc_ready = 0u;
}

static const system_floppy_controller_ops_t pc_fdc_ops = {
    SYSTEM_FLOPPY_ABI,
    pc_fdc_read_sector_buffer,
    pc_fdc_write_sector_buffer,
    pc_fdc_reset_state
};

void floppy_controller_platform_init(void) {
    (void)floppy_controller_register(&pc_fdc_ops);
}

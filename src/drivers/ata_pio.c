#include <avlos/storage.h>
#include <avlos/arch_x86_64.h>

#define ATA_DATA 0x1f0u
#define ATA_SECTOR_COUNT 0x1f2u
#define ATA_LBA_LOW 0x1f3u
#define ATA_LBA_MID 0x1f4u
#define ATA_LBA_HIGH 0x1f5u
#define ATA_DRIVE 0x1f6u
#define ATA_STATUS 0x1f7u
#define ATA_COMMAND 0x1f7u
#define ATA_CONTROL 0x3f6u
#define ATA_BUSY 0x80u
#define ATA_DRQ 0x08u
#define ATA_ERR 0x01u

static int ata_primary_master_ready;
static uint8_t ata_lock;
static uint16_t identify_words[256];

static void ata_delay_400ns(void) {
    (void)x86_inb(ATA_STATUS);
    (void)x86_inb(ATA_STATUS);
    (void)x86_inb(ATA_STATUS);
    (void)x86_inb(ATA_STATUS);
}

static int ata_wait(uint8_t require_drq) {
    for (uint32_t spin = 0; spin < 1000000u; ++spin) {
        uint8_t status = x86_inb(ATA_STATUS);
        if (status == 0 || status == 0xffu) {
            return 0;
        }
        if ((status & ATA_BUSY) != 0) {
            continue;
        }
        if ((status & ATA_ERR) != 0) {
            return 0;
        }
        if (!require_drq || (status & ATA_DRQ) != 0) {
            return 1;
        }
    }
    return 0;
}

void storage_init(void) {
    ata_primary_master_ready = 0;
    x86_outb(ATA_CONTROL, 0x02u);
    x86_outb(ATA_DRIVE, 0xa0u);
    ata_delay_400ns();
    x86_outb(ATA_SECTOR_COUNT, 0);
    x86_outb(ATA_LBA_LOW, 0);
    x86_outb(ATA_LBA_MID, 0);
    x86_outb(ATA_LBA_HIGH, 0);
    x86_outb(ATA_COMMAND, 0xecu);
    uint8_t status = x86_inb(ATA_STATUS);
    if (status == 0 || status == 0xffu) {
        return;
    }
    if (!ata_wait(1)) {
        return;
    }
    x86_insw(ATA_DATA, identify_words, 256);
    ata_primary_master_ready = 1;
    x86_outb(ATA_CONTROL, 0);
}

int storage_available(void) {
    return ata_primary_master_ready;
}

static int ata_transfer(uint32_t lba, void *buffer, int write) {
    if (!ata_primary_master_ready || buffer == 0 || lba >= (1u << 28)) {
        return -1;
    }
    while (__atomic_test_and_set(&ata_lock, __ATOMIC_ACQUIRE)) {
        __asm__ volatile("pause");
    }

    x86_outb(ATA_DRIVE, (uint8_t)(0xe0u | ((lba >> 24) & 0x0fu)));
    ata_delay_400ns();
    x86_outb(ATA_SECTOR_COUNT, 1);
    x86_outb(ATA_LBA_LOW, (uint8_t)lba);
    x86_outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    x86_outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    x86_outb(ATA_COMMAND, write ? 0x30u : 0x20u);

    int result = -1;
    if (ata_wait(1)) {
        if (write) {
            x86_outsw(ATA_DATA, buffer, 256);
            x86_outb(ATA_COMMAND, 0xe7u);
            result = ata_wait(0) ? 0 : -1;
        } else {
            x86_insw(ATA_DATA, buffer, 256);
            result = 0;
        }
    }
    __atomic_clear(&ata_lock, __ATOMIC_RELEASE);
    return result;
}

int ata_read_sector(uint32_t lba, void *buffer) {
    return ata_transfer(lba, buffer, 0);
}

int ata_write_sector(uint32_t lba, const void *buffer) {
    return ata_transfer(lba, (void *)buffer, 1);
}


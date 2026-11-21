#ifndef AVLOS_STORAGE_H
#define AVLOS_STORAGE_H

#include <stdint.h>

void storage_init(void);
int storage_available(void);
int ata_read_sector(uint32_t lba, void *buffer);
int ata_write_sector(uint32_t lba, const void *buffer);

#endif


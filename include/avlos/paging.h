#ifndef AVLOS_PAGING_H
#define AVLOS_PAGING_H

#include <stdint.h>

#define PAGE_PRESENT 0x001ull
#define PAGE_WRITE   0x002ull
#define PAGE_USER    0x004ull
#define PAGE_PWT     0x008ull
#define PAGE_PCD     0x010ull
#define PAGE_NX      (1ull << 63)

int paging_init(uint64_t hhdm_offset);
int paging_nx_available(void);
int page_map(uint64_t virtual_address, uint64_t physical_address, uint64_t flags);
int page_unmap(uint64_t virtual_address);
uint64_t page_translate(uint64_t virtual_address);
uint64_t paging_kernel_root(void);

#endif


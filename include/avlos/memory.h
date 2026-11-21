#ifndef AVLOS_MEMORY_H
#define AVLOS_MEMORY_H

#include <avlos/limine_requests.h>
#include <stddef.h>
#include <stdint.h>

int memory_init(const struct limine_memmap_response *map, uint64_t hhdm_offset);
uint64_t pmm_alloc_frame(void);
void pmm_free_frame(uint64_t physical_address);
uint64_t pmm_free_frames(void);
void *phys_to_virt(uint64_t physical_address);
uint64_t virt_to_phys(const void *virtual_address);
void *kmalloc(size_t size);
void kfree(void *pointer);
uint64_t memory_total_usable_bytes(void);

#endif


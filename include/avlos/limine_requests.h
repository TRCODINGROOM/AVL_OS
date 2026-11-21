#ifndef AVLOS_LIMINE_REQUESTS_H
#define AVLOS_LIMINE_REQUESTS_H

#include <stdint.h>
#include <limine.h>

extern volatile uint64_t avlos_limine_base_revision[3];
extern volatile struct limine_framebuffer_request avlos_framebuffer_request;
extern volatile struct limine_memmap_request avlos_memmap_request;
extern volatile struct limine_firmware_type_request avlos_firmware_type_request;
extern volatile struct limine_hhdm_request avlos_hhdm_request;
extern volatile struct limine_paging_mode_request avlos_paging_mode_request;
extern volatile struct limine_mp_request avlos_mp_request;
extern volatile struct limine_rsdp_request avlos_rsdp_request;

#endif


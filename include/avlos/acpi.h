#ifndef AVLOS_ACPI_H
#define AVLOS_ACPI_H

#include <stdint.h>

int acpi_init(void *rsdp);
int acpi_available(void);
void acpi_shutdown(void) __attribute__((noreturn));
void acpi_reboot(void) __attribute__((noreturn));

#endif


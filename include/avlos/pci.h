#ifndef AVLOS_PCI_H
#define AVLOS_PCI_H

#include <stdint.h>

struct pci_device {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint8_t class_code;
    uint8_t subclass;
    uint16_t vendor_id;
    uint16_t device_id;
};

uint32_t pci_config_read32(uint8_t bus, uint8_t device,
                           uint8_t function, uint8_t offset);
void pci_config_write32(uint8_t bus, uint8_t device,
                        uint8_t function, uint8_t offset, uint32_t value);
int pci_find_device(uint16_t vendor, uint16_t device_id,
                    struct pci_device *result);

#endif


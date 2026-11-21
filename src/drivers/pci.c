#include <avlos/pci.h>
#include <avlos/arch_x86_64.h>

uint32_t pci_config_read32(uint8_t bus, uint8_t device,
                           uint8_t function, uint8_t offset) {
    uint32_t address = 0x80000000u |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)device << 11) |
                       ((uint32_t)function << 8) |
                       (offset & 0xfcu);
    x86_outl(0xcf8, address);
    return x86_inl(0xcfc);
}

void pci_config_write32(uint8_t bus, uint8_t device,
                        uint8_t function, uint8_t offset, uint32_t value) {
    uint32_t address = 0x80000000u |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)device << 11) |
                       ((uint32_t)function << 8) |
                       (offset & 0xfcu);
    x86_outl(0xcf8, address);
    x86_outl(0xcfc, value);
}

int pci_find_device(uint16_t vendor, uint16_t device_id,
                    struct pci_device *result) {
    for (uint32_t bus = 0; bus < 256; ++bus) {
        for (uint32_t device = 0; device < 32; ++device) {
            uint32_t identity =
                pci_config_read32((uint8_t)bus, (uint8_t)device, 0, 0);
            if ((identity & 0xffffu) == 0xffffu) {
                continue;
            }
            uint32_t header =
                pci_config_read32((uint8_t)bus, (uint8_t)device, 0, 0x0c);
            uint32_t functions = ((header >> 23) & 1u) != 0 ? 8u : 1u;
            for (uint32_t function = 0; function < functions; ++function) {
                identity = pci_config_read32((uint8_t)bus, (uint8_t)device,
                                             (uint8_t)function, 0);
                uint16_t found_vendor = (uint16_t)identity;
                uint16_t found_device = (uint16_t)(identity >> 16);
                if (found_vendor != vendor || found_device != device_id) {
                    continue;
                }
                if (result != 0) {
                    uint32_t class_info =
                        pci_config_read32((uint8_t)bus, (uint8_t)device,
                                          (uint8_t)function, 0x08);
                    result->bus = (uint8_t)bus;
                    result->device = (uint8_t)device;
                    result->function = (uint8_t)function;
                    result->vendor_id = found_vendor;
                    result->device_id = found_device;
                    result->class_code = (uint8_t)(class_info >> 24);
                    result->subclass = (uint8_t)(class_info >> 16);
                }
                return 1;
            }
        }
    }
    return 0;
}


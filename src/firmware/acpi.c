#include <avlos/acpi.h>
#include <avlos/arch_x86_64.h>
#include <avlos/memory.h>
#include <avlos/interrupts.h>

#define ACPI_MAX_TABLE_SIZE (1024u * 1024u)
#define ACPI_PM1_SLP_TYP_MASK (7u << 10)
#define ACPI_PM1_SLP_EN (1u << 13)

struct acpi_sdt_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

struct acpi_gas {
    uint8_t address_space;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;
    uint64_t address;
} __attribute__((packed));

struct acpi_region {
    uint8_t address_space;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;
    uint64_t address;
};

static struct acpi_region pm1a_control;
static struct acpi_region pm1b_control;
static struct acpi_region reset_register;
static uint8_t reset_value;
static uint8_t sleep_type_a;
static uint8_t sleep_type_b;
static int acpi_ready;

static uint8_t sum_bytes(const uint8_t *bytes, uint32_t length) {
    uint8_t sum = 0;
    for (uint32_t index = 0; index < length; ++index) {
        sum = (uint8_t)(sum + bytes[index]);
    }
    return sum;
}

static int signature_is(const char *actual, const char *expected) {
    for (uint32_t index = 0; index < 4; ++index) {
        if (actual[index] != expected[index]) {
            return 0;
        }
    }
    return 1;
}

static struct acpi_sdt_header *map_sdt(uint64_t physical) {
    if (physical == 0) {
        return 0;
    }
    struct acpi_sdt_header *header =
        (struct acpi_sdt_header *)phys_to_virt(physical);
    if (header->length < sizeof(*header) ||
        header->length > ACPI_MAX_TABLE_SIZE ||
        sum_bytes((const uint8_t *)header, header->length) != 0) {
        return 0;
    }
    return header;
}

static struct acpi_sdt_header *find_table(struct acpi_sdt_header *root,
                                           uint32_t entry_size,
                                           const char *signature) {
    if (root == 0 || root->length < sizeof(*root) || entry_size == 0) {
        return 0;
    }
    uint32_t payload = root->length - (uint32_t)sizeof(*root);
    for (uint32_t offset = 0; offset + entry_size <= payload;
         offset += entry_size) {
        uint64_t physical = 0;
        const uint8_t *entry = (const uint8_t *)root + sizeof(*root) + offset;
        if (entry_size == 8) {
            for (uint32_t byte = 0; byte < 8; ++byte) {
                physical |= (uint64_t)entry[byte] << (byte * 8u);
            }
        } else {
            for (uint32_t byte = 0; byte < 4; ++byte) {
                physical |= (uint64_t)entry[byte] << (byte * 8u);
            }
        }
        struct acpi_sdt_header *candidate = map_sdt(physical);
        if (candidate != 0 && signature_is(candidate->signature, signature)) {
            return candidate;
        }
    }
    return 0;
}

static uint32_t read_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static uint64_t read_u64(const uint8_t *bytes) {
    uint64_t value = 0;
    for (uint32_t index = 0; index < 8; ++index) {
        value |= (uint64_t)bytes[index] << (index * 8u);
    }
    return value;
}

static int aml_integer(const uint8_t *bytes, uint32_t end,
                       uint32_t *position, uint64_t *value) {
    if (*position >= end) {
        return 0;
    }
    uint8_t opcode = bytes[(*position)++];
    if (opcode == 0x00u) {
        *value = 0;
        return 1;
    }
    if (opcode == 0x01u) {
        *value = 1;
        return 1;
    }
    if (opcode == 0xffu) {
        *value = UINT64_MAX;
        return 1;
    }
    uint32_t size = opcode == 0x0au ? 1u :
                    opcode == 0x0bu ? 2u :
                    opcode == 0x0cu ? 4u :
                    opcode == 0x0eu ? 8u : 0u;
    if (size == 0 || *position + size > end) {
        return 0;
    }
    *value = 0;
    for (uint32_t index = 0; index < size; ++index) {
        *value |= (uint64_t)bytes[(*position)++] << (index * 8u);
    }
    return 1;
}

static int find_s5_package(const uint8_t *aml, uint32_t length,
                           uint8_t *type_a, uint8_t *type_b) {
    for (uint32_t index = 1; index + 8u < length; ++index) {
        if (aml[index - 1u] != 0x08u || aml[index] != '_' ||
            aml[index + 1u] != 'S' || aml[index + 2u] != '5' ||
            aml[index + 3u] != '_' || aml[index + 4u] != 0x12u) {
            continue;
        }

        uint32_t position = index + 5u;
        uint32_t package_start = position;
        uint8_t encoded_length = aml[position++];
        uint32_t follow_bytes = encoded_length >> 6;
        uint32_t package_length = encoded_length & (follow_bytes == 0 ? 0x3fu : 0x0fu);
        for (uint32_t byte = 0; byte < follow_bytes; ++byte) {
            if (position >= length) {
                return 0;
            }
            package_length |= (uint32_t)aml[position++] << (4u + byte * 8u);
        }
        uint32_t package_end = package_start + package_length;
        if (package_end > length || position >= package_end) {
            continue;
        }
        uint8_t elements = aml[position++];
        uint64_t first = 0;
        uint64_t second = 0;
        if (elements < 2u ||
            !aml_integer(aml, package_end, &position, &first) ||
            !aml_integer(aml, package_end, &position, &second) ||
            first > 7u || second > 7u) {
            continue;
        }
        *type_a = (uint8_t)first;
        *type_b = (uint8_t)second;
        return 1;
    }
    return 0;
}

static struct acpi_region gas_from(const uint8_t *bytes) {
    struct acpi_region region = {0};
    const struct acpi_gas *gas = (const struct acpi_gas *)bytes;
    region.address_space = gas->address_space;
    region.bit_width = gas->bit_width;
    region.bit_offset = gas->bit_offset;
    region.access_size = gas->access_size;
    region.address = gas->address;
    return region;
}

static int region_valid(struct acpi_region region) {
    return region.address != 0 &&
           (region.address_space == 0 || region.address_space == 1);
}

static uint32_t region_width_bytes(struct acpi_region region) {
    if (region.access_size == 1) return 1;
    if (region.access_size == 2) return 2;
    if (region.access_size == 3) return 4;
    if (region.access_size == 4) return 8;
    if (region.bit_width <= 8) return 1;
    if (region.bit_width <= 16) return 2;
    if (region.bit_width <= 32) return 4;
    return 8;
}

static uint64_t region_read(struct acpi_region region) {
    uint32_t width = region_width_bytes(region);
    if (region.address_space == 1) {
        if (width == 1) return x86_inb((uint16_t)region.address);
        if (width == 2) return x86_inw((uint16_t)region.address);
        if (width == 4) return x86_inl((uint16_t)region.address);
        return 0;
    }
    volatile uint8_t *address = (volatile uint8_t *)phys_to_virt(region.address);
    if (width == 1) return *address;
    if (width == 2) return *(volatile uint16_t *)(void *)address;
    if (width == 4) return *(volatile uint32_t *)(void *)address;
    return *(volatile uint64_t *)(void *)address;
}

static int region_write(struct acpi_region region, uint64_t value) {
    if (!region_valid(region)) {
        return 0;
    }
    uint32_t width = region_width_bytes(region);
    uint64_t shift = region.bit_offset;
    if (shift != 0) {
        value <<= shift;
    }
    if (region.address_space == 1) {
        if (width == 1) x86_outb((uint16_t)region.address, (uint8_t)value);
        else if (width == 2) x86_outw((uint16_t)region.address, (uint16_t)value);
        else if (width == 4) x86_outl((uint16_t)region.address, (uint32_t)value);
        else return 0;
        return 1;
    }
    volatile uint8_t *address = (volatile uint8_t *)phys_to_virt(region.address);
    if (width == 1) *address = (uint8_t)value;
    else if (width == 2) *(volatile uint16_t *)(void *)address = (uint16_t)value;
    else if (width == 4) *(volatile uint32_t *)(void *)address = (uint32_t)value;
    else *(volatile uint64_t *)(void *)address = value;
    return 1;
}

static struct acpi_sdt_header *find_dsdt(uint8_t *fadt, uint32_t length) {
    uint64_t physical = length >= 148u ? read_u64(fadt + 140u) : 0;
    if (physical == 0 && length >= 44u) {
        physical = read_u32(fadt + 40u);
    }
    struct acpi_sdt_header *dsdt = map_sdt(physical);
    return dsdt != 0 && signature_is(dsdt->signature, "DSDT") ? dsdt : 0;
}

int acpi_init(void *rsdp_pointer) {
    acpi_ready = 0;
    if (rsdp_pointer == 0) {
        return -1;
    }
    const uint8_t *rsdp = (const uint8_t *)rsdp_pointer;
    static const char rsdp_signature[8] = {'R','S','D',' ','P','T','R',' '};
    for (uint32_t index = 0; index < 8; ++index) {
        if (rsdp[index] != (uint8_t)rsdp_signature[index]) return -1;
    }
    if (sum_bytes(rsdp, 20u) != 0) return -1;

    uint8_t revision = rsdp[15];
    uint64_t root_physical = read_u32(rsdp + 16u);
    uint32_t root_entry_size = 4;
    if (revision >= 2u) {
        uint32_t rsdp_length = read_u32(rsdp + 20u);
        if (rsdp_length >= 36u && rsdp_length <= 4096u &&
            sum_bytes(rsdp, rsdp_length) == 0) {
            uint64_t xsdt = read_u64(rsdp + 24u);
            if (xsdt != 0) {
                root_physical = xsdt;
                root_entry_size = 8;
            }
        }
    }

    struct acpi_sdt_header *root = map_sdt(root_physical);
    if (root == 0 ||
        !signature_is(root->signature, root_entry_size == 8 ? "XSDT" : "RSDT")) {
        return -1;
    }
    struct acpi_sdt_header *fadt_header =
        find_table(root, root_entry_size, "FACP");
    if (fadt_header == 0 || fadt_header->length < 90u) return -1;

    uint8_t *fadt = (uint8_t *)fadt_header;
    uint32_t length = fadt_header->length;
    pm1a_control = length >= 184u ? gas_from(fadt + 172u) : (struct acpi_region){0};
    pm1b_control = length >= 196u ? gas_from(fadt + 184u) : (struct acpi_region){0};
    if (!region_valid(pm1a_control)) {
        pm1a_control.address_space = 1;
        pm1a_control.bit_width = fadt[89u] * 8u;
        pm1a_control.address = read_u32(fadt + 64u);
    }
    if (!region_valid(pm1b_control)) {
        pm1b_control.address_space = 1;
        pm1b_control.bit_width = fadt[89u] * 8u;
        pm1b_control.address = read_u32(fadt + 68u);
    }
    if (!region_valid(pm1a_control)) return -1;

    if (length >= 129u) {
        reset_register = gas_from(fadt + 116u);
        reset_value = fadt[128u];
    }

    struct acpi_sdt_header *dsdt = find_dsdt(fadt, length);
    if (dsdt == 0 ||
        !find_s5_package((const uint8_t *)dsdt + sizeof(*dsdt),
                         dsdt->length - (uint32_t)sizeof(*dsdt),
                         &sleep_type_a, &sleep_type_b)) {
        return -1;
    }

    acpi_ready = 1;
    if (fadt[89u] >= 2u && (region_read(pm1a_control) & 1u) == 0) {
        uint32_t smi_command = read_u32(fadt + 48u);
        uint8_t acpi_enable = fadt[52u];
        if (smi_command != 0 && acpi_enable != 0) {
            x86_outb((uint16_t)smi_command, acpi_enable);
            for (uint32_t spin = 0; spin < 1000000u; ++spin) {
                if ((region_read(pm1a_control) & 1u) != 0) break;
                __asm__ volatile("pause");
            }
        }
    }
    return 0;
}

int acpi_available(void) {
    return acpi_ready;
}

void acpi_shutdown(void) {
    interrupts_disable();
    if (acpi_ready) {
        uint64_t value_a = region_read(pm1a_control);
        value_a &= ~(uint64_t)ACPI_PM1_SLP_TYP_MASK;
        value_a |= ((uint64_t)sleep_type_a << 10) | ACPI_PM1_SLP_EN;
        (void)region_write(pm1a_control, value_a);
        if (region_valid(pm1b_control)) {
            uint64_t value_b = region_read(pm1b_control);
            value_b &= ~(uint64_t)ACPI_PM1_SLP_TYP_MASK;
            value_b |= ((uint64_t)sleep_type_b << 10) | ACPI_PM1_SLP_EN;
            (void)region_write(pm1b_control, value_b);
        }
    }
    for (;;) __asm__ volatile("cli; hlt" : : : "memory");
}

void acpi_reboot(void) {
    interrupts_disable();
    if (region_valid(reset_register) &&
        region_write(reset_register, reset_value)) {
        for (uint32_t spin = 0; spin < 1000000u; ++spin) {
            __asm__ volatile("pause");
        }
    }
    for (uint32_t spin = 0; spin < 100000u; ++spin) {
        if ((x86_inb(0x64u) & 0x02u) == 0) break;
        __asm__ volatile("pause");
    }
    x86_outb(0x64u, 0xfeu);
    for (;;) __asm__ volatile("cli; hlt" : : : "memory");
}

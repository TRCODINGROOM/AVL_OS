#include <avlos/paging.h>
#include <avlos/memory.h>
#include <avlos/arch_x86_64.h>

#define PAGE_SIZE 4096ull
#define PAGE_ADDRESS_MASK 0x000ffffffffff000ull
#define PAGE_HUGE 0x080ull

static uint64_t root_physical;
static uint8_t page_table_lock;
static int nx_available;

static void lock_page_tables(void) {
    while (__atomic_test_and_set(&page_table_lock, __ATOMIC_ACQUIRE)) {
        __asm__ volatile("pause");
    }
}

static void unlock_page_tables(void) {
    __atomic_clear(&page_table_lock, __ATOMIC_RELEASE);
}

static uint64_t *table_from_entry(uint64_t entry) {
    return (uint64_t *)phys_to_virt(entry & PAGE_ADDRESS_MASK);
}

static uint64_t *walk_to_leaf(uint64_t virtual_address, int create, int user) {
    uint64_t *table = (uint64_t *)phys_to_virt(root_physical);
    const uint16_t indexes[3] = {
        (uint16_t)((virtual_address >> 39) & 0x1ffu),
        (uint16_t)((virtual_address >> 30) & 0x1ffu),
        (uint16_t)((virtual_address >> 21) & 0x1ffu),
    };

    for (uint32_t level = 0; level < 3; ++level) {
        uint64_t *entry = &table[indexes[level]];
        if ((*entry & PAGE_PRESENT) == 0) {
            if (!create) {
                return 0;
            }
            uint64_t new_table = pmm_alloc_frame();
            if (new_table == 0) {
                return 0;
            }
            uint64_t *new_table_virtual = (uint64_t *)phys_to_virt(new_table);
            for (uint32_t item = 0; item < 512; ++item) {
                new_table_virtual[item] = 0;
            }
            *entry = new_table | PAGE_PRESENT | PAGE_WRITE |
                     (user ? PAGE_USER : 0);
        } else {
            if ((*entry & PAGE_HUGE) != 0) {
                return 0;
            }
            *entry |= PAGE_WRITE;
            if (user) {
                *entry |= PAGE_USER;
            }
        }
        table = table_from_entry(*entry);
    }

    return &table[(virtual_address >> 12) & 0x1ffu];
}

int paging_init(uint64_t hhdm) {
    if (hhdm == 0) {
        return -1;
    }
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    root_physical = cr3 & PAGE_ADDRESS_MASK;
    uint32_t maximum_leaf;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    __asm__ volatile("cpuid"
                     : "=a"(maximum_leaf), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(0x80000000u), "c"(0));
    (void)ebx;
    (void)ecx;
    if (maximum_leaf >= 0x80000001u) {
        __asm__ volatile("cpuid"
                         : "=a"(maximum_leaf), "=b"(ebx), "=c"(ecx), "=d"(edx)
                         : "a"(0x80000001u), "c"(0));
        if ((edx & (1u << 20)) != 0) {
            uint64_t efer = arch_rdmsr(0xc0000080u);
            arch_wrmsr(0xc0000080u, efer | (1ull << 11));
            nx_available = 1;
        }
    }
    return root_physical == 0 ? -1 : 0;
}

int paging_nx_available(void) {
    return nx_available;
}

int page_map(uint64_t virtual_address, uint64_t physical_address, uint64_t flags) {
    if ((virtual_address & (PAGE_SIZE - 1u)) != 0 ||
        (physical_address & (PAGE_SIZE - 1u)) != 0 ||
        (physical_address & ~PAGE_ADDRESS_MASK) != 0) {
        return -1;
    }

    lock_page_tables();
    int user = (flags & PAGE_USER) != 0;
    uint64_t *leaf = walk_to_leaf(virtual_address, 1, user);
    if (leaf == 0) {
        unlock_page_tables();
        return -1;
    }
    if ((*leaf & PAGE_PRESENT) != 0 &&
        (*leaf & PAGE_ADDRESS_MASK) != physical_address) {
        unlock_page_tables();
        return -1;
    }
    *leaf = physical_address | (flags & ~PAGE_ADDRESS_MASK) | PAGE_PRESENT;
    unlock_page_tables();
    __asm__ volatile("invlpg (%0)" : : "r"(virtual_address) : "memory");
    return 0;
}

int page_unmap(uint64_t virtual_address) {
    if ((virtual_address & (PAGE_SIZE - 1u)) != 0) {
        return -1;
    }
    lock_page_tables();
    uint64_t *leaf = walk_to_leaf(virtual_address, 0, 0);
    if (leaf == 0 || (*leaf & PAGE_PRESENT) == 0) {
        unlock_page_tables();
        return -1;
    }
    *leaf = 0;
    unlock_page_tables();
    __asm__ volatile("invlpg (%0)" : : "r"(virtual_address) : "memory");
    return 0;
}

uint64_t page_translate(uint64_t virtual_address) {
    lock_page_tables();
    uint64_t *table = (uint64_t *)phys_to_virt(root_physical);
    uint16_t pml4_index = (uint16_t)((virtual_address >> 39) & 0x1ffu);
    uint16_t pdpt_index = (uint16_t)((virtual_address >> 30) & 0x1ffu);
    uint16_t pd_index = (uint16_t)((virtual_address >> 21) & 0x1ffu);
    uint16_t pt_index = (uint16_t)((virtual_address >> 12) & 0x1ffu);
    uint64_t entry = table[pml4_index];
    if ((entry & PAGE_PRESENT) == 0 || (entry & PAGE_HUGE) != 0) {
        unlock_page_tables();
        return 0;
    }
    table = table_from_entry(entry);
    entry = table[pdpt_index];
    if ((entry & PAGE_PRESENT) == 0) {
        unlock_page_tables();
        return 0;
    }
    if ((entry & PAGE_HUGE) != 0) {
        uint64_t result = (entry & 0x000fffffc0000000ull) |
                          (virtual_address & 0x3fffffffull);
        unlock_page_tables();
        return result;
    }
    table = table_from_entry(entry);
    entry = table[pd_index];
    if ((entry & PAGE_PRESENT) == 0) {
        unlock_page_tables();
        return 0;
    }
    if ((entry & PAGE_HUGE) != 0) {
        uint64_t result = (entry & 0x000fffffffe00000ull) |
                          (virtual_address & 0x1fffffull);
        unlock_page_tables();
        return result;
    }
    table = table_from_entry(entry);
    entry = table[pt_index];
    unlock_page_tables();
    return (entry & PAGE_PRESENT) != 0
               ? (entry & PAGE_ADDRESS_MASK) | (virtual_address & 0xfffull)
               : 0;
}

uint64_t paging_kernel_root(void) {
    return root_physical;
}


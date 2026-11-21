#include <avlos/memory.h>
#include <avlos/arch_x86_64.h>

#define PAGE_SIZE 4096ull
#define SLAB_MAGIC 0x534c4142u
#define LARGE_MAGIC 0x4c415247u
#define SLAB_CLASSES 8u
#define SLAB_DATA_OFFSET 64u
#define LARGE_LIMIT (PAGE_SIZE - SLAB_DATA_OFFSET)

struct frame_node {
    struct frame_node *next;
};

struct slab_page {
    uint32_t magic;
    uint16_t class_index;
    uint16_t stride;
    uint32_t free_count;
    uint32_t block_count;
    void *free_head;
    struct slab_page *next;
};

struct large_page {
    uint32_t magic;
    uint32_t reserved;
    uint64_t requested_size;
};

static const uint16_t slab_sizes[SLAB_CLASSES] = {
    16, 32, 64, 128, 256, 512, 1024, 2048
};

static struct frame_node *free_frames;
static uint64_t free_frame_count;
static uint64_t usable_bytes;
static uint64_t hhdm_base;
static uint8_t pmm_lock;
static uint8_t heap_lock;
static struct slab_page *slab_lists[SLAB_CLASSES];

static void lock_byte(uint8_t *lock) {
    while (__atomic_test_and_set(lock, __ATOMIC_ACQUIRE)) {
        __asm__ volatile("pause");
    }
}

static void unlock_byte(uint8_t *lock) {
    __atomic_clear(lock, __ATOMIC_RELEASE);
}

static uint64_t align_up(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

int memory_init(const struct limine_memmap_response *map, uint64_t hhdm_offset) {
    if (map == 0 || map->entries == 0 || hhdm_offset == 0) {
        return -1;
    }

    free_frames = 0;
    free_frame_count = 0;
    usable_bytes = 0;
    hhdm_base = hhdm_offset;
    for (uint64_t index = 0; index < map->entry_count; ++index) {
        const struct limine_memmap_entry *entry = map->entries[index];
        if (entry == 0 || entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        usable_bytes += entry->length;
        uint64_t start = align_up(entry->base, PAGE_SIZE);
        uint64_t end = (entry->base + entry->length) & ~(PAGE_SIZE - 1u);
        if (start < 0x100000ull) {
            start = 0x100000ull;
        }
        if (end <= start) {
            continue;
        }

        for (uint64_t physical = start; physical < end; physical += PAGE_SIZE) {
            struct frame_node *node = (struct frame_node *)(uintptr_t)(hhdm_base + physical);
            node->next = free_frames;
            free_frames = node;
            ++free_frame_count;
        }
    }

    return free_frame_count == 0 ? -1 : 0;
}

uint64_t pmm_alloc_frame(void) {
    lock_byte(&pmm_lock);
    struct frame_node *node = free_frames;
    if (node != 0) {
        free_frames = node->next;
        --free_frame_count;
    }
    unlock_byte(&pmm_lock);
    if (node == 0) {
        return 0;
    }
    return (uint64_t)(uintptr_t)node - hhdm_base;
}

void pmm_free_frame(uint64_t physical_address) {
    if (physical_address == 0 || (physical_address & (PAGE_SIZE - 1u)) != 0) {
        return;
    }
    struct frame_node *node =
        (struct frame_node *)(uintptr_t)(hhdm_base + physical_address);
    lock_byte(&pmm_lock);
    node->next = free_frames;
    free_frames = node;
    ++free_frame_count;
    unlock_byte(&pmm_lock);
}

uint64_t pmm_free_frames(void) {
    lock_byte(&pmm_lock);
    uint64_t count = free_frame_count;
    unlock_byte(&pmm_lock);
    return count;
}

void *phys_to_virt(uint64_t physical_address) {
    return (void *)(uintptr_t)(hhdm_base + physical_address);
}

uint64_t virt_to_phys(const void *virtual_address) {
    uint64_t address = (uint64_t)(uintptr_t)virtual_address;
    return address >= hhdm_base ? address - hhdm_base : 0;
}

uint64_t memory_total_usable_bytes(void) {
    return usable_bytes;
}

static int class_for_size(size_t size) {
    for (uint32_t index = 0; index < SLAB_CLASSES; ++index) {
        if (size <= slab_sizes[index]) {
            return (int)index;
        }
    }
    return -1;
}

static struct slab_page *new_slab(uint32_t class_index) {
    uint64_t physical = pmm_alloc_frame();
    if (physical == 0) {
        return 0;
    }
    uint8_t *base = (uint8_t *)phys_to_virt(physical);
    struct slab_page *slab = (struct slab_page *)base;
    slab->magic = SLAB_MAGIC;
    slab->class_index = (uint16_t)class_index;
    slab->stride = slab_sizes[class_index];
    slab->free_count = 0;
    slab->block_count = 0;
    slab->free_head = 0;
    slab->next = slab_lists[class_index];

    uint32_t stride = slab->stride;
    uint32_t count = (uint32_t)((PAGE_SIZE - SLAB_DATA_OFFSET) / stride);
    for (uint32_t index = 0; index < count; ++index) {
        void *block = base + SLAB_DATA_OFFSET + (uint64_t)index * stride;
        *(void **)block = slab->free_head;
        slab->free_head = block;
        ++slab->free_count;
    }
    slab->block_count = count;
    slab_lists[class_index] = slab;
    return slab;
}

void *kmalloc(size_t size) {
    if (size == 0) {
        return 0;
    }
    if (size > LARGE_LIMIT) {
        return 0;
    }

    lock_byte(&heap_lock);
    int class_index = class_for_size(size);
    if (class_index < 0) {
        uint64_t physical = pmm_alloc_frame();
        if (physical == 0) {
            unlock_byte(&heap_lock);
            return 0;
        }
        uint8_t *base = (uint8_t *)phys_to_virt(physical);
        struct large_page *header = (struct large_page *)base;
        header->magic = LARGE_MAGIC;
        header->reserved = 0;
        header->requested_size = size;
        unlock_byte(&heap_lock);
        return base + SLAB_DATA_OFFSET;
    }

    struct slab_page *slab = slab_lists[class_index];
    while (slab != 0 && slab->free_head == 0) {
        slab = slab->next;
    }
    if (slab == 0) {
        slab = new_slab((uint32_t)class_index);
    }
    if (slab == 0 || slab->free_head == 0) {
        unlock_byte(&heap_lock);
        return 0;
    }

    void *block = slab->free_head;
    slab->free_head = *(void **)block;
    --slab->free_count;
    unlock_byte(&heap_lock);
    return block;
}

void kfree(void *pointer) {
    if (pointer == 0) {
        return;
    }

    uintptr_t page_base = (uintptr_t)pointer & ~(uintptr_t)(PAGE_SIZE - 1u);
    uint32_t magic = *(uint32_t *)page_base;
    if (magic == LARGE_MAGIC) {
        ((struct large_page *)page_base)->magic = 0;
        pmm_free_frame(virt_to_phys((void *)page_base));
        return;
    }
    if (magic != SLAB_MAGIC) {
        return;
    }

    struct slab_page *slab = (struct slab_page *)page_base;
    uint32_t class_index = slab->class_index;
    if (class_index >= SLAB_CLASSES) {
        return;
    }
    lock_byte(&heap_lock);
    *(void **)pointer = slab->free_head;
    slab->free_head = pointer;
    ++slab->free_count;
    unlock_byte(&heap_lock);
}


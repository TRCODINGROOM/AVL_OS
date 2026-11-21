#include "rtl8139.h"
#include <avlos/pci.h>
#include <avlos/arch_x86_64.h>
#include <avlos/paging.h>

#define RTL_VENDOR 0x10ecu
#define RTL_DEVICE 0x8139u
#define RX_BUFFER_SIZE 8192u
#define RX_BUFFER_PAD 16u
#define TX_BUFFER_SIZE 2048u
#define TX_BUFFER_COUNT 4u

enum {
    RTL_IDR0 = 0x00,
    RTL_TSD0 = 0x10,
    RTL_TSAD0 = 0x20,
    RTL_RBSTART = 0x30,
    RTL_CR = 0x37,
    RTL_CAPR = 0x38,
    RTL_IMR = 0x3c,
    RTL_ISR = 0x3e,
    RTL_TCR = 0x40,
    RTL_RCR = 0x44,
    RTL_CONFIG1 = 0x52,
};

static struct pci_device rtl_device;
static uint16_t rtl_io_base;
static uint8_t rtl_mac[6];
static uint8_t rx_ring[RX_BUFFER_SIZE + RX_BUFFER_PAD] __attribute__((aligned(256)));
static uint8_t tx_buffers[TX_BUFFER_COUNT][TX_BUFFER_SIZE] __attribute__((aligned(16)));
static uint8_t rx_scratch[TX_BUFFER_SIZE];
static uint16_t rx_offset;
static uint8_t tx_slot;
static int rtl_ready;

static uint8_t reg_read8(uint16_t offset) {
    return x86_inb(rtl_io_base + offset);
}

static uint16_t reg_read16(uint16_t offset) {
    return x86_inw(rtl_io_base + offset);
}

static uint32_t reg_read32(uint16_t offset) {
    return x86_inl(rtl_io_base + offset);
}

static void reg_write8(uint16_t offset, uint8_t value) {
    x86_outb(rtl_io_base + offset, value);
}

static void reg_write16(uint16_t offset, uint16_t value) {
    x86_outw(rtl_io_base + offset, value);
}

static void reg_write32(uint16_t offset, uint32_t value) {
    x86_outl(rtl_io_base + offset, value);
}

int rtl8139_init(void) {
    rtl_ready = 0;
    if (!pci_find_device(RTL_VENDOR, RTL_DEVICE, &rtl_device)) {
        return 0;
    }
    uint32_t bar0 = pci_config_read32(rtl_device.bus, rtl_device.device,
                                      rtl_device.function, 0x10);
    if ((bar0 & 1u) == 0) {
        return 0;
    }
    rtl_io_base = (uint16_t)(bar0 & 0xfffcu);
    uint32_t command = pci_config_read32(rtl_device.bus, rtl_device.device,
                                         rtl_device.function, 0x04);
    command |= 0x00000005u;
    pci_config_write32(rtl_device.bus, rtl_device.device,
                       rtl_device.function, 0x04, command);

    reg_write8(RTL_CONFIG1, 0);
    reg_write8(RTL_CR, 0x10u);
    uint32_t timeout = 1000000u;
    while ((reg_read8(RTL_CR) & 0x10u) != 0 && timeout-- != 0) {
        __asm__ volatile("pause");
    }
    if (timeout == 0) {
        return 0;
    }

    uint64_t rx_physical = page_translate((uint64_t)(uintptr_t)rx_ring);
    if (rx_physical == 0) {
        return 0;
    }
    reg_write32(RTL_RBSTART, (uint32_t)rx_physical);
    reg_write16(RTL_IMR, 0);
    reg_write16(RTL_ISR, 0xffffu);
    reg_write32(RTL_RCR, 0x0000008fu);
    reg_write32(RTL_TCR, 0x03000700u);

    for (uint32_t index = 0; index < 6; ++index) {
        rtl_mac[index] = reg_read8(RTL_IDR0 + index);
    }
    for (uint32_t index = 0; index < TX_BUFFER_COUNT; ++index) {
        uint64_t physical =
            page_translate((uint64_t)(uintptr_t)tx_buffers[index]);
        if (physical == 0) {
            return 0;
        }
        reg_write32(RTL_TSAD0 + index * 4u, (uint32_t)physical);
    }

    rx_offset = 0;
    tx_slot = 0;
    reg_write16(RTL_CAPR, 0xfff0u);
    reg_write8(RTL_CR, 0x0cu);
    rtl_ready = 1;
    return 1;
}

int rtl8139_send(const void *frame, size_t length) {
    if (!rtl_ready || frame == 0 || length < 14 || length > TX_BUFFER_SIZE) {
        return -1;
    }
    uint8_t slot = tx_slot;
    tx_slot = (uint8_t)((tx_slot + 1u) % TX_BUFFER_COUNT);
    const uint8_t *source = (const uint8_t *)frame;
    for (size_t index = 0; index < length; ++index) {
        tx_buffers[slot][index] = source[index];
    }
    reg_write32(RTL_TSD0 + slot * 4u, (uint32_t)length);
    uint32_t timeout = 1000000u;
    while (timeout-- != 0) {
        uint32_t status = reg_read32(RTL_TSD0 + slot * 4u);
        if ((status & (1u << 15)) != 0) {
            return 0;
        }
        if ((status & (1u << 14)) != 0) {
            return -1;
        }
        __asm__ volatile("pause");
    }
    return -1;
}

int rtl8139_receive(void *frame, size_t capacity, size_t *length) {
    if (!rtl_ready || frame == 0 || length == 0) {
        return 0;
    }
    if ((reg_read8(RTL_CR) & 0x01u) != 0) {
        return 0;
    }

    uint16_t packet_status =
        *(volatile uint16_t *)(void *)(rx_ring + rx_offset);
    uint16_t packet_length =
        *(volatile uint16_t *)(void *)(rx_ring + rx_offset + 2u);
    if ((packet_status & 0x01u) == 0 || packet_length < 4u ||
        packet_length > TX_BUFFER_SIZE) {
        rx_offset = 0;
        reg_write16(RTL_CAPR, 0xfff0u);
        return 0;
    }

    size_t frame_length = packet_length - 4u;
    if (frame_length > capacity) {
        frame_length = capacity;
    }
    for (size_t index = 0; index < frame_length; ++index) {
        rx_scratch[index] =
            rx_ring[(rx_offset + 4u + (uint32_t)index) % RX_BUFFER_SIZE];
    }
    uint8_t *destination = (uint8_t *)frame;
    for (size_t index = 0; index < frame_length; ++index) {
        destination[index] = rx_scratch[index];
    }
    *length = frame_length;

    uint32_t next = (rx_offset + packet_length + 4u + 3u) & ~3u;
    rx_offset = (uint16_t)(next % RX_BUFFER_SIZE);
    reg_write16(RTL_CAPR, (uint16_t)(rx_offset - 16u));
    return 1;
}

const uint8_t *rtl8139_mac_address(void) {
    return rtl_mac;
}


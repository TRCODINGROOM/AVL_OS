#include <avlos/network.h>
#include <avlos/arch_x86_64.h>
#include "../drivers/rtl8139.h"

#define ETHERNET_HEADER_SIZE 14u
#define NETWORK_BUFFER_SIZE 2048u

static uint8_t local_mac[6];
static const uint8_t local_ip[4] = {10, 0, 2, 15};
static uint8_t receive_buffer[NETWORK_BUFFER_SIZE];
static volatile uint64_t received_frames;
static volatile uint64_t transmitted_frames;
static int network_ready;

static uint16_t read_be16(const uint8_t *bytes) {
    return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

static void write_be16(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)value;
}

static uint16_t checksum(const uint8_t *bytes, size_t length) {
    uint32_t sum = 0;
    size_t index = 0;
    while (index + 1 < length) {
        sum += ((uint32_t)bytes[index] << 8) | bytes[index + 1];
        index += 2;
    }
    if (index < length) {
        sum += (uint32_t)bytes[index] << 8;
    }
    while ((sum >> 16) != 0) {
        sum = (sum & 0xffffu) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

static int is_local_ip(const uint8_t *ip) {
    return ip[0] == local_ip[0] && ip[1] == local_ip[1] &&
           ip[2] == local_ip[2] && ip[3] == local_ip[3];
}

static void handle_arp(const uint8_t *frame, size_t length) {
    if (length < 42u || read_be16(frame + 14) != 1u ||
        read_be16(frame + 16) != 0x0800u ||
        frame[18] != 6u || frame[19] != 4u ||
        read_be16(frame + 20) != 1u || !is_local_ip(frame + 38)) {
        return;
    }

    uint8_t reply[42];
    for (uint32_t index = 0; index < 6; ++index) {
        reply[index] = frame[22 + index];
        reply[6 + index] = local_mac[index];
        reply[22 + index] = local_mac[index];
        reply[32 + index] = frame[22 + index];
    }
    reply[12] = 0x08;
    reply[13] = 0x06;
    write_be16(reply + 14, 1);
    write_be16(reply + 16, 0x0800);
    reply[18] = 6;
    reply[19] = 4;
    write_be16(reply + 20, 2);
    for (uint32_t index = 0; index < 4; ++index) {
        reply[28 + index] = local_ip[index];
        reply[38 + index] = frame[28 + index];
    }
    (void)network_send_frame(reply, sizeof(reply));
}

static void handle_icmp_echo(const uint8_t *frame, size_t length) {
    if (length < 34u || (frame[14] >> 4) != 4u) {
        return;
    }
    uint32_t ip_header_length = (frame[14] & 0x0fu) * 4u;
    uint16_t total_length = read_be16(frame + 16);
    if (ip_header_length < 20u || total_length < ip_header_length + 8u ||
        length < ETHERNET_HEADER_SIZE + total_length ||
        frame[23] != 1u || !is_local_ip(frame + 30)) {
        return;
    }

    uint32_t icmp_offset = ETHERNET_HEADER_SIZE + ip_header_length;
    if (frame[icmp_offset] != 8u) {
        return;
    }
    uint8_t reply[NETWORK_BUFFER_SIZE];
    for (uint32_t index = 0; index < ETHERNET_HEADER_SIZE + total_length; ++index) {
        reply[index] = frame[index];
    }
    for (uint32_t index = 0; index < 6; ++index) {
        reply[index] = frame[6 + index];
        reply[6 + index] = local_mac[index];
    }
    for (uint32_t index = 0; index < 4; ++index) {
        reply[26 + index] = local_ip[index];
        reply[30 + index] = frame[26 + index];
    }

    write_be16(reply + 24, 0);
    write_be16(reply + 24, checksum(reply + 14, ip_header_length));
    reply[icmp_offset] = 0;
    write_be16(reply + icmp_offset + 2, 0);
    write_be16(reply + icmp_offset + 2,
               checksum(reply + icmp_offset, total_length - ip_header_length));
    (void)network_send_frame(reply, ETHERNET_HEADER_SIZE + total_length);
}

static void receive_frame(const uint8_t *frame, size_t length) {
    if (length < ETHERNET_HEADER_SIZE) {
        return;
    }
    uint16_t ether_type = read_be16(frame + 12);
    if (ether_type == 0x0806u) {
        handle_arp(frame, length);
    } else if (ether_type == 0x0800u) {
        handle_icmp_echo(frame, length);
    }
}

void network_init(void) {
    network_ready = rtl8139_init();
    if (!network_ready) {
        return;
    }
    const uint8_t *mac = rtl8139_mac_address();
    for (uint32_t index = 0; index < 6; ++index) {
        local_mac[index] = mac[index];
    }
}

void network_poll(void) {
    if (!network_ready) {
        return;
    }
    for (uint32_t count = 0; count < 16u; ++count) {
        size_t length = 0;
        if (!rtl8139_receive(receive_buffer, sizeof(receive_buffer), &length)) {
            break;
        }
        __atomic_add_fetch(&received_frames, 1, __ATOMIC_RELAXED);
        receive_frame(receive_buffer, length);
    }
}

int network_available(void) {
    return network_ready;
}

int network_send_frame(const void *frame, size_t length) {
    if (!network_ready || rtl8139_send(frame, length) != 0) {
        return -1;
    }
    __atomic_add_fetch(&transmitted_frames, 1, __ATOMIC_RELAXED);
    return 0;
}

uint64_t network_rx_frames(void) {
    return __atomic_load_n(&received_frames, __ATOMIC_RELAXED);
}

uint64_t network_tx_frames(void) {
    return __atomic_load_n(&transmitted_frames, __ATOMIC_RELAXED);
}


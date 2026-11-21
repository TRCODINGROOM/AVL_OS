#ifndef AVLOS_RTL8139_H
#define AVLOS_RTL8139_H

#include <stddef.h>
#include <stdint.h>

int rtl8139_init(void);
int rtl8139_send(const void *frame, size_t length);
int rtl8139_receive(void *frame, size_t capacity, size_t *length);
const uint8_t *rtl8139_mac_address(void);

#endif


#ifndef AVLOS_NETWORK_H
#define AVLOS_NETWORK_H

#include <stddef.h>
#include <stdint.h>

void network_init(void);
void network_poll(void);
int network_available(void);
int network_send_frame(const void *frame, size_t length);
uint64_t network_rx_frames(void);
uint64_t network_tx_frames(void);

#endif


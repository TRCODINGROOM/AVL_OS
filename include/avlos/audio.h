#ifndef AVLOS_AUDIO_H
#define AVLOS_AUDIO_H

#include <stdint.h>

void audio_init(void);
void audio_beep(uint32_t frequency_hz, uint32_t duration_ms);
void audio_stop(void);

#endif


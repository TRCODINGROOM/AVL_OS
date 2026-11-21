#ifndef AVLOS_INPUT_H
#define AVLOS_INPUT_H

#include <stdint.h>

enum input_event_type {
    INPUT_EVENT_KEY = 1,
    INPUT_EVENT_MOUSE = 2,
};

struct input_event {
    uint8_t type;
    uint8_t code;
    uint8_t pressed;
    uint8_t buttons;
    int16_t dx;
    int16_t dy;
};

void input_init(void);
void input_keyboard_irq(void);
void input_mouse_irq(void);
int input_pop(struct input_event *event);

#endif


#ifndef AVLOS_GUI_H
#define AVLOS_GUI_H

#include <avlos/kernel.h>
#include <avlos/input.h>

void gui_init(const struct avlos_boot_info *boot_info);
void gui_handle_event(const struct input_event *event);
void gui_redraw(void);
void gui_process_events(void);

#endif


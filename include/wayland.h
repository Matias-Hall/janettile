#ifndef WAYLAND_H
#define WAYLAND_H

#include <stdbool.h>
#include "view.h"

// Initialises all wayland stuff.
bool init_wayland(void);

void add_layout_handler(LayoutHandler layout_function);

void add_command_handler(CommandHandler command_function);

// Finishes all wayland stuff.
void finish_wayland(void);

// Processes the next wayland event.
// Returns true if there are more events to process.
bool continue_wayland(void);

#endif

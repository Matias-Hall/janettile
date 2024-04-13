#ifndef JANET_PROCESSOR_H
#define JANET_PROCESSOR_H

#include <stdbool.h>
#include <stdint.h>
#include "view.h"

bool init_janet(char *filename);

void evaluate_command(const char *command);

struct view *produce_layout(uint32_t n, uint32_t width, uint32_t height);

void finish_janet(void);

#endif

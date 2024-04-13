#ifndef VIEW_H
#define VIEW_H

#include <stdint.h>

struct view
{
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
};

typedef struct view *(*LayoutHandler)(uint32_t view_count, uint32_t width, uint32_t height);

// Doesn't really fit here.
typedef void (*CommandHandler)(const char *command);

#endif

/*
 * Tiled layout for river, implemented in understandable, simple, commented code.
 * Reading this code should help you get a basic understanding of how to use
 * river-layout to create a basic layout generator.
 *
 * Q: Wow, this is a lot of code just for a layout!
 * A: No, it really is not. Most of the code here is just generic Wayland client
 *    boilerplate. The actual layout part is pretty small.
 *
 * Q: Can I use this to port dwm layouts to river?
 * A: Yes you can! You just need to replace the logic in layout_handle_layout_demand().
 *    You don't even need to fully understand the protocol if all you want to
 *    do is just port some layouts.
 *
 * Q: I have no idea how any of this works.
 * A: If all you want to do is create layouts, you do not need to understand
 *    the Wayland parts of the code. If you still want to understand it and are
 *    familiar with how Wayland clients work, read the protocol. If you are new
 *    to writing Wayland client code, you can read https://wayland-book.com,
 *    then read the protocol.
 *
 * Q: How do I build this?
 * A: To build, you need to generate the header and code of the layout protocol
 *    extension and link against them. This is achieved with the following
 *    commands (You may want to setup a build system).
 *
 *        wayland-scanner private-code < river-layout-v3.xml > river-layout-v3.c
 *        wayland-scanner client-header < river-layout-v3.xml > river-layout-v3.h
 *        gcc -Wall -Wextra -Wpedantic -Wno-unused-parameter -c -o layout.o layout.c
 *        gcc -Wall -Wextra -Wpedantic -Wno-unused-parameter -c -o river-layout-v3.o river-layout-v3.c
 *        gcc -o layout layout.o river-layout-v3.o -lwayland-client
 */

#include <stdio.h>
#include <stdlib.h>
#include "wayland.h"
#include "janet_processor.h"
#include "view.h"


int main (int argc, char *argv[])
{
	int exit_code = 0;
	if (argc >= 2) {
		if (init_janet(argv[1])) {
		/* if (init_janet(argv[1]) && init_wayland()) { */
		/* 	add_layout_handler(&produce_layout); */
		/* 	add_command_handler(&evaluate_command); */
		/* 	while ( continue_wayland() ); */
		/* } */
			if (init_wayland()) {
				add_layout_handler(&produce_layout);
				add_command_handler(&evaluate_command);
				while (continue_wayland());
			} else {
				exit_code = 1;
			}

			finish_wayland();
			struct view *views = produce_layout(3, 1920, 1080);
			if (views == NULL) {
				printf("Views is null.\n");
			} else {
				for (uintptr_t i = 0; i < 3; i++) {
					printf("{%d %d %d %d}", views[i].x, views[i].y, views[i].width, views[i].height);
				}
			}
			free(views);
			finish_janet();
		} else {
			exit_code = 1;
		}
	} else {
		fprintf(stderr, "Filename missing.\n");
		exit_code = 1;
	}
	return exit_code;
}

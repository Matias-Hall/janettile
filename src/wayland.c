#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wayland-client.h>
#include <wayland-client-protocol.h>

#include "river-layout-v3.h"

#include "view.h"

#include "wayland.h"

struct output
{
	struct wl_list link;

	struct wl_output       *output;
	struct river_layout_v3 *layout;
};

/* In Wayland it's a good idea to have your main data global, since you'll need
 * it everywhere anyway.
 */
static struct wl_display  *wl_display;
static struct wl_registry *wl_registry;
static struct wl_callback *sync_callback;
static struct river_layout_manager_v3 *layout_manager;
static struct wl_list outputs;

static LayoutHandler create_layout;
static CommandHandler handle_command;

static bool failure = false;

static void layout_handle_layout_demand(void *data, struct river_layout_v3 *river_layout_v3,
		uint32_t view_count, uint32_t width, uint32_t height, uint32_t tags, uint32_t serial)
{
	if (create_layout == NULL)
		return;

	struct output *output = (struct output *)data;

	struct view *views = create_layout(view_count, width, height);

	// TODO: Some sort of check on the length of views.
	// Maybe janet_processor.c code should check that the correct count is returned.
	for (uintptr_t i = 0; i < view_count; i++) {
		struct view view = views[i];
		river_layout_v3_push_view_dimensions(output->layout, view.x, view.y, view.width, view.height, serial);
	}

	/* Committing the layout means telling the server that your code is done
	 * laying out windows. Make sure you have pushed exactly the right
	 * amount of view dimensions, a mismatch is a protocol error.
	 *
	 * You also have to provide a layout name. This is a user facing string
	 * that the server can forward to status bars. You can use it to tell
	 * the user which layout is currently in use. You could also add some
	 * status information about your layout, but in this example we are
	 * boring and just use a static "[]=" like in dwm.
	 */
	river_layout_v3_commit(output->layout, "[]=", serial);

	free(views);
}

static void layout_handle_namespace_in_use(void *data, struct river_layout_v3 *river_layout_v3)
{
	/* Oh no, the namespace we choose is already used by another client!
	 * All we can do now is destroy the river_layout object. Because we are
	 * lazy, we just abort and let our cleanup mechanism destroy it. A more
	 * sophisticated client could instead destroy only the one single
	 * affected river_layout object and recover from this mishap. Writing
	 * such a client is left as an exercise for the reader.
	 */
	fputs("Namespace already in use.\n", stderr);
	failure = true;
}

static void layout_handle_user_command(void *data, struct river_layout_v3 *river_layout_manager_v3,
		const char *_command)
{
	/* The user_command event will be received whenever the user decided to
	 * send us a command. As an example, commands can be used to change the
	 * layout values. Parsing the commands is the job of the layout
	 * generator, the server just sends us the raw string.
	 *
	 * After this event is recevied, the views on the output will be
	 * re-arranged and so we will also receive a layout_demand event.
	 */

	struct output *output = (struct output *)data;

	handle_command(_command);

	/* Skip preceding whitespace. */
	/* char *command = (char *)_command; */
	/* if (! skip_whitespace(&command)) */
	/* 	return; */

	/* if (word_comp(command, "main_count")) */
	/* 	handle_uint32_command(&command, &output->main_count, "main_count"); */
	/* else if (word_comp(command, "view_padding")) */
	/* 	handle_uint32_command(&command, &output->view_padding, "view_padding"); */
	/* else if (word_comp(command, "outer_padding")) */
	/* 	handle_uint32_command(&command, &output->outer_padding, "outer_padding"); */
	/* else if (word_comp(command, "main_ratio")) */
	/* 	handle_float_command(&command, &output->main_ratio, "main_ratio", 0.1, 0.9); */
	/* else if (word_comp(command, "reset")) */
	/* { */
	/* 	/1* This is an example of a command that does something different */
	/* 	 * than just modifying a value. It resets all values to their */
	/* 	 * defaults. */
	/* 	 *1/ */

	/* 	if ( skip_nonwhitespace(&command) && skip_whitespace(&command) ) */
	/* 	{ */
	/* 		fputs("ERROR: Too many arguments. 'reset' has no arguments.\n", stderr); */
	/* 		return; */
	/* 	} */

	/* 	output->main_count    = 1; */
	/* 	output->main_ratio    = 0.6; */
	/* 	output->view_padding  = 5; */
	/* 	output->outer_padding = 5; */
	/* } */
	/* else */
	/* 	fprintf(stderr, "ERROR: Unknown command: %s\n", command); */
}

static const struct river_layout_v3_listener layout_listener = {
	.namespace_in_use = layout_handle_namespace_in_use,
	.layout_demand    = layout_handle_layout_demand,
	.user_command     = layout_handle_user_command,
};

static void configure_output(struct output *output)
{
	/* The namespace of the layout is how the compositor chooses what layout
	 * to use. It can be any arbitrary string. It should describe roughly
	 * what kind of layout your client will create, so here we use "tile".
	 */
	output->layout = river_layout_manager_v3_get_layout(layout_manager,
			output->output, "janettile");
	river_layout_v3_add_listener(output->layout, &layout_listener, output);
}

static bool create_output(struct wl_output *wl_output)
{
	struct output *output = calloc(1, sizeof(struct output));
	if ( output == NULL )
	{
		fputs("Failed to allocate.\n", stderr);
		return false;
	}

	output->output     = wl_output;
	output->layout     = NULL;

	/* These are the parameters of our layout. In this case, they are the
	 * ones you'd typically expect from a dynamic tiling layout, but if you
	 * are creative, you can do more. You can use any arbitrary amount of
	 * all kinds of values in your layout. If the user wants to change a
	 * value, the server lets us know using user_command event of the
	 * river_layout object.
	 *
	 * A layout generator is responsible for having sane defaults for all
	 * layout values. The server only sends user_command events when there
	 * actually is a command the user wants to send us.
	 */
	/* output->main_count    = 1; */
	/* output->main_ratio    = 0.6; */
	/* output->view_padding  = 5; */
	/* output->outer_padding = 5; */

	wl_list_insert(&outputs, &output->link);
	return true;
}

static void destroy_output(struct output *output)
{
	if ( output->layout != NULL )
		river_layout_v3_destroy(output->layout);
	wl_output_destroy(output->output);
	wl_list_remove(&output->link);
	free(output);
}

static void destroy_all_outputs()
{
	struct output *output, *tmp;
	wl_list_for_each_safe(output, tmp, &outputs, link)
		destroy_output(output);
}

static void registry_handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version)
{
	if ( strcmp(interface, river_layout_manager_v3_interface.name) == 0 )
		layout_manager = wl_registry_bind(registry, name,
				&river_layout_manager_v3_interface, 1);
	else if ( strcmp(interface, wl_output_interface.name) == 0 )
	{
		struct wl_output *wl_output = wl_registry_bind(registry, name,
				&wl_output_interface, version);
		if (! create_output(wl_output))
		{
			failure = true;
		}
	}
}

/* A no-op function we plug into listeners when we don't want to handle an event. */
static void noop () {}

static const struct wl_registry_listener registry_listener = {
	.global        = registry_handle_global,
	.global_remove = noop
};

static void sync_handle_done(void *data, struct wl_callback *wl_callback,
		uint32_t irrelevant)
{
	wl_callback_destroy(wl_callback);
	sync_callback = NULL;

	/* When this function is called, the registry finished advertising all
	 * available globals. Let's check if we have everything we need.
	 */
	if ( layout_manager == NULL )
	{
		fputs("Wayland compositor does not support river-layout-v3.\n", stderr);
		failure = true;
		return;
	}

	/* Configure all outputs here */
	struct output *output;
	wl_list_for_each(output, &outputs, link)
		configure_output(output);
}

static const struct wl_callback_listener sync_callback_listener = {
	.done = sync_handle_done,
};

bool init_wayland(void)
{
	/* We query the display name here instead of letting wl_display_connect()
	 * figure it out itself, because libwayland (for legacy reasons) falls
	 * back to using "wayland-0" when $WAYLAND_DISPLAY is not set, which is
	 * generally not desirable.
	 */
	const char *display_name = getenv("WAYLAND_DISPLAY");
	if ( display_name == NULL )
	{
		fputs("WAYLAND_DISPLAY is not set.\n", stderr);
		return false;
	}

	wl_display = wl_display_connect(display_name);
	if ( wl_display == NULL )
	{
		fputs("Can not connect to Wayland server.\n", stderr);
		return false;
	}

	wl_list_init(&outputs);

	/* The registry is a global object which is used to advertise all
	 * available global objects.
	 */
	wl_registry = wl_display_get_registry(wl_display);
	wl_registry_add_listener(wl_registry, &registry_listener, NULL);

	/* The sync callback we attach here will be called when all previous
	 * requests have been handled by the server. This allows us to know the
	 * end of the startup, at which point all necessary globals should be
	 * bound.
	 */
	sync_callback = wl_display_sync(wl_display);
	wl_callback_add_listener(sync_callback, &sync_callback_listener, NULL);

	return true;
}

void add_layout_handler(LayoutHandler handler)
{
	create_layout = handler;
}

void add_command_handler(CommandHandler handler)
{
	handle_command = handler;
}


void finish_wayland(void)
{
	if ( wl_display == NULL )
		return;

	destroy_all_outputs();

	if ( sync_callback != NULL )
		wl_callback_destroy(sync_callback);
	if ( layout_manager != NULL )
		river_layout_manager_v3_destroy(layout_manager);

	wl_registry_destroy(wl_registry);
	wl_display_disconnect(wl_display);
}

bool continue_wayland(void)
{
	return !(failure || (wl_display_dispatch(wl_display) == -1));
}

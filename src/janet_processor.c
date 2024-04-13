#include <janet.h>

#include "view.h"
#include "process_janet.h"

#include "janet_processor.h"

static JanetFunction *janetfn_evaluate_file;
static JanetFunction *janetfn_evaluate_command;
static JanetFunction *janetfn_layout;
static JanetFunction *janetfn_pp;
static JanetTable *environment;

static struct view table_to_view(JanetTable *table)
{
	struct view view = {
		.x = janet_unwrap_integer(janet_table_get(table, janet_ckeywordv("x"))),
		.y = janet_unwrap_integer(janet_table_get(table, janet_ckeywordv("y"))),
		.width = janet_unwrap_integer(janet_table_get(table, janet_ckeywordv("width"))),
		.height = janet_unwrap_integer(janet_table_get(table, janet_ckeywordv("height"))),
	};
	return view;
}

static struct view struct_to_view(JanetStruct st)
{
	struct view view = {
		.x = janet_unwrap_integer(janet_struct_get(st, janet_ckeywordv("x"))),
		.y = janet_unwrap_integer(janet_struct_get(st, janet_ckeywordv("y"))),
		.width = janet_unwrap_integer(janet_struct_get(st, janet_ckeywordv("width"))),
		.height = janet_unwrap_integer(janet_struct_get(st, janet_ckeywordv("height"))),
	};
	return view;
}

static bool call_fn(JanetFunction *fn, int argc, const Janet *argv, Janet *out)
{
	JanetFiber *fiber = NULL;
	if (janet_pcall(fn, argc, argv, out, &fiber) == JANET_SIGNAL_OK) {
		return true;
	} else {
		janet_stacktrace(fiber, *out);
		return false;
	}
}

static bool get_layout_function(void)
{
	Janet layout_function;
	janet_resolve(environment, janet_csymbol("layout"), &layout_function);

	if (janet_checktype(layout_function, JANET_FUNCTION)) {
		janetfn_layout = janet_unwrap_function(layout_function);
		return true;
	} else {
		return false;
	}
}

static bool evaluate_file(char *filename)
{
	if (environment != NULL) {
		janet_gcunroot(janet_wrap_table(environment));
		environment = NULL;
	}

	const Janet args[1] = { janet_cstringv(filename) };

	Janet env;
	if (!call_fn(janetfn_evaluate_file, 1, args, &env)) {
		return false;
	}

	janet_gcroot(env);
	environment = janet_unwrap_table(env);

	if (!get_layout_function()) {
		fprintf(stderr, "%s must include a layout function.\n", filename);
		return false;
	}

	return true;
}

void evaluate_command(const char *command)
{
	// TODO: Check that init_janet has been called, (easiest way by checking janetfn_evalaute_command is not null).
	const Janet args[2] = { janet_wrap_table(environment), janet_cstringv(command) };
	// While the function does return an environment, this is the same one that is passed in,
	// so it is unnecessary to do anything with it.
	Janet env;
	// TODO: Deal with errors.
	call_fn(janetfn_evaluate_command, 2, args, &env);

	// TODO: Deal with no layout function found.
	get_layout_function();
}

struct view *produce_layout(uint32_t n, uint32_t width, uint32_t height)
{
	if (janetfn_layout == NULL)
		return NULL;

	const Janet args[3] = { janet_wrap_integer(n), janet_wrap_integer(width), janet_wrap_integer(height) };

	Janet out;
	if (!call_fn(janetfn_layout, 3, args, &out))
		// TODO: Deal with errors.
		return NULL;

	if (janet_checktype(out, JANET_ARRAY)) {
		JanetArray *view_array = janet_unwrap_array(out);

		// Number of views created must match the requested amount.
		if (view_array->count != n) {
			fprintf(stderr, "%d views must be created, but %d were received\n", n, view_array->count);
			return NULL;
		}

		struct view *views = malloc(view_array->count * sizeof(struct view));
		for (uintptr_t i = 0; i < view_array->count; i++) {
			Janet view = view_array->data[i];
			if (janet_checktype(view, JANET_TABLE)) {
				views[i] = table_to_view(janet_unwrap_table(view));
			} else if (janet_checktype(view, JANET_STRUCT)) {
				views[i] = struct_to_view(janet_unwrap_struct(view));
			} else {
				fprintf(stderr, "Expected a table or struct but received a different value\n");
				// FIXME: Undefined behaviour as views[i] won't have a correct value.
			}
		}
		return views;
	} else {
		fprintf(stderr, "Expected an array but received a different value\n");
		return NULL;
	}
}

bool init_janet(char *filename)
{
	janet_init();

	// Get core env
	JanetTable *env = janet_core_env(NULL);
	JanetTable *lookup = janet_env_lookup(env);

	// TODO: Look into janet_dostring/janet_dobytes to replace this.
	Janet processor_environment = janet_unmarshal(process_jimage, process_jimage_len, 0, lookup, NULL);


	JanetTable *env_table = janet_unwrap_table(processor_environment);

	Janet function;

	janet_resolve(env_table, janet_csymbol("evaluate-file"), &function);
	// Make sure function is not garbage collected.
	janet_gcroot(function);
	janetfn_evaluate_file = janet_unwrap_function(function);

	janet_resolve(env_table, janet_csymbol("evaluate-command"), &function);
	// Make sure function is not garbage collected.
	janet_gcroot(function);
	janetfn_evaluate_command = janet_unwrap_function(function);

	// Used for debugging.
	janet_resolve(env_table, janet_csymbol("pp"), &function);
	// Make sure function is not garbage collected.
	janet_gcroot(function);
	janetfn_pp = janet_unwrap_function(function);

	return evaluate_file(filename);
}

void finish_janet(void)
{
	if (janetfn_evaluate_file)
		janet_gcunroot(janet_wrap_function(janetfn_evaluate_file));
	if (janetfn_evaluate_command)
		janet_gcunroot(janet_wrap_function(janetfn_evaluate_command));
	if (janetfn_pp)
		janet_gcunroot(janet_wrap_function(janetfn_pp));
	if (environment)
		janet_gcunroot(janet_wrap_table(environment));
	janet_deinit();
}

#include "editor-window.h"
#include <obs-frontend-api.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("hltn-advanced", "en-US")

static adv_editor *g_editor = nullptr;

static void tools_callback(void *)
{
	if (g_editor) {
		editor_close(g_editor);
		g_editor = nullptr;
	}
	g_editor = editor_open();
}

static void frontend_event(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		obs_frontend_add_tools_menu_item("Advance Display Output",
			tools_callback, nullptr);
	}
}

bool obs_module_load(void)
{
	obs_frontend_add_event_callback(frontend_event, nullptr);
	return true;
}

void obs_module_unload(void)
{
	if (g_editor) {
		editor_close(g_editor);
		g_editor = nullptr;
	}
}

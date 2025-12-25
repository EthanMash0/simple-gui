#include "state.h"

#include "config.h"
#include "dock.h"
#include "hypr_events.h"

HyprdockState *hyprdock_state_new(GtkWidget *dock_box)
{
    HyprdockState *st = g_new0(HyprdockState, 1);

    st->dock_box = dock_box;

    // Load config
    st->cfg = dock_config_load();

    // CSS provider (create + attach; then load your style.css)
    st->css = dock_css_provider_create_and_attach();
    dock_css_provider_reload(st->css);

    // runtime init
    st->items = NULL;
    st->poll_id = 0;
    st->refresh_pending = 0;
    st->event_thread = NULL;
		st->monitors = g_ptr_array_new_with_free_func(g_object_unref);
		st->event_fd = -1;
		st->stop_requested = 0;
		st->refresh_idle_id = 0;

    return st;
}

void hyprdock_state_free(HyprdockState *st)
{
    if (!st) return;

    // Stop subsystems first (they may schedule work against the state)
    hypr_events_stop(st);   // safe no-op if not started
    dock_shutdown(st);      // frees st->items, etc. (safe no-op if NULL)

    if (st->cfg) {
        dock_config_free(st->cfg);
        st->cfg = NULL;
    }

    if (st->css) {
        g_object_unref(st->css);
        st->css = NULL;
    }

		if (st->monitors) g_ptr_array_free(st->monitors, TRUE);

    g_free(st);
}

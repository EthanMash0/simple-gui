#include "watch.h"

#include <gio/gio.h>
#include <glib.h>

#include "config.h"   // dock_find_config_path, dock_css_provider_reload
#include "dock.h"     // idle_rebuild_config
#include "state.h"    // HyprdockState

static gboolean should_handle_event(GFileMonitorEvent ev) {
    return ev == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT ||
           ev == G_FILE_MONITOR_EVENT_CREATED ||
           ev == G_FILE_MONITOR_EVENT_MOVED_IN;
}

void on_config_file_changed(GFileMonitor *mon,
                            GFile *file,
                            GFile *other,
                            GFileMonitorEvent ev,
                            gpointer user_data)
{
    (void)mon; (void)file; (void)other;

    HyprdockState *st = (HyprdockState *)user_data;
    if (!st) return;
    if (!should_handle_event(ev)) return;

    // Rebuild on the main loop (and pass st through!)
    g_idle_add(idle_rebuild_config, st);
}

void on_style_file_changed(GFileMonitor *mon,
                           GFile *file,
                           GFile *other,
                           GFileMonitorEvent ev,
                           gpointer user_data)
{
    (void)mon; (void)file; (void)other;

    HyprdockState *st = (HyprdockState *)user_data;
    if (!st) return;
    if (!should_handle_event(ev)) return;

    if (st->css) {
        dock_css_provider_reload(st->css);
    }
}

void watch_user_file(const char *name, GCallback cb, gpointer user_data)
{
    gchar *path = dock_find_config_path(name);
    if (!path) return;

    GFile *f = g_file_new_for_path(path);
    g_free(path);

    GError *err = NULL;
    GFileMonitor *m = g_file_monitor_file(f, G_FILE_MONITOR_NONE, NULL, &err);
    g_object_unref(f);

    if (!m) {
        if (err) {
            g_warning("monitor failed: %s", err->message);
            g_error_free(err);
        }
        return;
    }

    // Optional: reduce event spam
    g_file_monitor_set_rate_limit(m, 200);

    // IMPORTANT: pass user_data through
    g_signal_connect(m, "changed", cb, user_data);

    // IMPORTANT: keep the monitor alive.
    // Best practice: store it on the state so it is unref'd during shutdown.
    HyprdockState *st = (HyprdockState *)user_data;
    if (st && st->monitors) {
        // st->monitors should be a GPtrArray with free func g_object_unref
        g_ptr_array_add(st->monitors, m); // transfers our ref into the array
    } else {
        // Fallback: leak intentionally for process lifetime
        // (still safe for single-window apps that exit on close)
        (void)m;
    }
}

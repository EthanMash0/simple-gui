#include "desktop_match.h"

#include <gio-unix-2.0/gio/gdesktopappinfo.h>
#include <glib.h>
#include <string.h>

char *desktop_match_key(const char *desktop_id) {
    if (!desktop_id || !*desktop_id) return g_strdup("");

    GDesktopAppInfo *app = g_desktop_app_info_new(desktop_id);
    if (app) {
        const char *wm = g_desktop_app_info_get_string(app, "StartupWMClass");
        if (wm && *wm) {
            char *k = g_ascii_strdown(wm, -1);
            g_object_unref(app);
            return k;
        }
        g_object_unref(app);
    }

    if (g_str_has_suffix(desktop_id, ".desktop")) {
        size_t n = strlen(desktop_id) - strlen(".desktop");
        char *base = g_strndup(desktop_id, (gsize)n);
        char *k = g_ascii_strdown(base, -1);
        g_free(base);
        return k;
    }

    return g_ascii_strdown(desktop_id, -1);
}

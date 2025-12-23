// src/main.c
#include <glib-2.0/glib.h>
#define _GNU_SOURCE

#include <gtk/gtk.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

#include <gio-unix-2.0/gio/gdesktopappinfo.h>
#include <glib.h>

#include <cairo.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>

#include "config.h"
#include "hypr.h"

// ---------------------------
// App state (file-scope)
// ---------------------------

typedef struct {
    char *desktop_id;   // e.g. "firefox.desktop"
    char *match_key;    // lowercased StartupWMClass or desktop-id fallback
    GtkWidget *dot;     // indicator widget
} DockItem;

static DockConfig      *g_cfg   = NULL;
static GtkCssProvider  *g_css   = NULL;

static GtkWidget       *g_box   = NULL;     // holds app buttons
static GPtrArray       *g_items = NULL;     // DockItem*

static guint            g_poll_id = 0;
static gint							g_refresh_pending = 0;

// ---------------------------
// Forward declarations
// ---------------------------

static gboolean refresh_running(gpointer user_data);
static void schedule_refresh(void);
static GtkWidget* make_app_widget(const char *desktop_id, int icon_size);

static void rebuild_dock_from_config(void);

static gpointer hypr_event_thread(gpointer data);

static void watch_user_file(const char *name, GCallback cb);
static void on_style_file_changed(GFileMonitor *mon, GFile *file, GFile *other,
                                  GFileMonitorEvent ev, gpointer user_data);
static void on_config_file_changed(GFileMonitor *mon, GFile *file, GFile *other,
                                   GFileMonitorEvent ev, gpointer user_data);

// ---------------------------
// Utilities
// ---------------------------

static void dock_item_free(gpointer p) {
    DockItem *it = (DockItem*)p;
    if (!it) return;
    g_free(it->desktop_id);
    g_free(it->match_key);
    g_free(it);
}

static void rebuild_items_array(void) {
    if (g_items) g_ptr_array_free(g_items, TRUE);
    g_items = g_ptr_array_new_with_free_func(dock_item_free);
}

static void clear_box(GtkWidget *box) {
    GtkWidget *child = gtk_widget_get_first_child(box);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(box), child);
        child = next;
    }
}

static void ensure_polling_fallback(void) {
    if (g_poll_id != 0) return;
    g_poll_id = g_timeout_add_seconds(1, (GSourceFunc)refresh_running, NULL);
}

static gboolean refresh_idle_cb(gpointer data) {
    (void)data;
    g_atomic_int_set(&g_refresh_pending, 0);
    refresh_running(NULL);
    return G_SOURCE_REMOVE;
}

static void schedule_refresh(void) {
    if (!g_atomic_int_compare_and_exchange(&g_refresh_pending, 0, 1))
        return;
    g_idle_add((GSourceFunc)refresh_idle_cb, NULL);
}


// ---------------------------
// Indicator dot (drawn)
// ---------------------------

static GtkWidget* make_dot(void) {
    // GtkWidget *d = gtk_drawing_area_new();
		GtkWidget *d = gtk_frame_new(NULL);
		gtk_widget_add_css_class(d, "indicator");
    gtk_widget_set_size_request(d, 5, 9);
    gtk_widget_set_halign(d, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(d, GTK_ALIGN_END);
    // gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(d), dot_draw, NULL, NULL);
    gtk_widget_set_visible(d, FALSE);
    return d;
}

// ---------------------------
// Desktop entry matching
// ---------------------------

static char* desktop_match_key(const char *desktop_id) {
    // Prefer StartupWMClass if present
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

    // Fallback: desktop_id without ".desktop"
    const char *dot = g_strrstr(desktop_id, ".desktop");
    if (dot && dot > desktop_id) {
        char *base = g_strndup(desktop_id, (gsize)(dot - desktop_id));
        char *k = g_ascii_strdown(base, -1);
        g_free(base);
        return k;
    }

    return g_ascii_strdown(desktop_id, -1);
}

// ---------------------------
// Launch callback
// ---------------------------

static void on_app_clicked(GtkButton *b, gpointer user_data) {
    (void)b;
    const char *desktop_id = (const char*)user_data;

    GDesktopAppInfo *app = g_desktop_app_info_new(desktop_id);
    if (!app) {
        g_warning("No desktop entry found: %s", desktop_id);
        return;
    }

    GError *err = NULL;
    if (!g_app_info_launch(G_APP_INFO(app), NULL, NULL, &err)) {
        if (err) {
            g_warning("launch failed for %s: %s", desktop_id, err->message);
            g_error_free(err);
        }
    }
    g_object_unref(app);
}

// ---------------------------
// App widget builder
// ---------------------------

static GtkWidget* make_app_widget(const char *desktop_id, int icon_size) {
    GtkWidget *btn = gtk_button_new();
    gtk_button_set_has_frame(GTK_BUTTON(btn), FALSE);
    gtk_widget_add_css_class(btn, "icon");

    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign(v, GTK_ALIGN_CENTER);

    // icon (image)
    GtkWidget *img = NULL;
    GDesktopAppInfo *app = g_desktop_app_info_new(desktop_id);
    if (app) {
        GIcon *gicon = g_app_info_get_icon(G_APP_INFO(app));
        img = gtk_image_new_from_gicon(gicon);
        gtk_image_set_pixel_size(GTK_IMAGE(img), icon_size);
        g_object_unref(app);
    } else {
        img = gtk_label_new(desktop_id);
    }

    // indicator
    GtkWidget *dot = make_dot();

    gtk_box_append(GTK_BOX(v), img);
    gtk_box_append(GTK_BOX(v), dot);
    gtk_button_set_child(GTK_BUTTON(btn), v);

    // click -> launch
    g_signal_connect_data(
        btn, "clicked",
        G_CALLBACK(on_app_clicked),
        g_strdup(desktop_id),
        (GClosureNotify)g_free,
        0
    );

    // register for running refresh
    DockItem *it = g_new0(DockItem, 1);
    it->desktop_id = g_strdup(desktop_id);
    it->match_key  = desktop_match_key(desktop_id);
    it->dot        = dot;
    g_ptr_array_add(g_items, it);

    return btn;
}

// ---------------------------
// Running indicator refresh
// ---------------------------

static gboolean refresh_running(gpointer user_data) {
    (void)user_data;

    if (!g_items || g_items->len == 0) return G_SOURCE_CONTINUE;

		GHashTable *counts = hypr_get_running_class_counts();

		// g_message("running classes: %u", g_hash_table_size(counts));
    for (guint i = 0; i < g_items->len; i++) {
        DockItem *it = g_ptr_array_index(g_items, i);
				gpointer v = g_hash_table_lookup(counts, it->match_key);
				int c = v ? GPOINTER_TO_INT(v) : 0;
				gboolean on = (c > 0);
				// g_message("%s match_key=%s -> %d", it->desktop_id, it->match_key, c);
        gtk_widget_set_visible(it->dot, on);
    }

    g_hash_table_destroy(counts);
    return G_SOURCE_CONTINUE;
}

// ---------------------------
// Config + CSS reload
// ---------------------------

static void rebuild_dock_from_config(void) {
    DockConfig *newcfg = dock_config_load();

    clear_box(g_box);
    rebuild_items_array();

    if (newcfg->pinned_apps) {
        for (gchar **p = newcfg->pinned_apps; *p; p++) {
            if (**p == '\0') continue;
            GtkWidget *w = make_app_widget(*p, newcfg->icon_size);
            gtk_box_append(GTK_BOX(g_box), w);
        }
    }

    if (g_cfg) dock_config_free(g_cfg);
    g_cfg = newcfg;

    refresh_running(NULL);
}

static gboolean idle_rebuild_config(gpointer data) {
    (void)data;
    rebuild_dock_from_config();
    return G_SOURCE_REMOVE;
}

static void on_config_file_changed(GFileMonitor *mon, GFile *file, GFile *other,
                                   GFileMonitorEvent ev, gpointer user_data)
{
    (void)mon; (void)file; (void)other; (void)user_data;
    if (ev != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT &&
        ev != G_FILE_MONITOR_EVENT_CREATED &&
        ev != G_FILE_MONITOR_EVENT_MOVED_IN)
        return;

    g_idle_add(idle_rebuild_config, NULL);
}

static void on_style_file_changed(GFileMonitor *mon, GFile *file, GFile *other,
                                  GFileMonitorEvent ev, gpointer user_data)
{
    (void)mon; (void)file; (void)other; (void)user_data;
    if (ev != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT &&
        ev != G_FILE_MONITOR_EVENT_CREATED &&
        ev != G_FILE_MONITOR_EVENT_MOVED_IN)
        return;

    if (g_css) dock_css_provider_reload(g_css);
}

static void watch_user_file(const char *name, GCallback cb) {
    gchar *path = dock_user_config_path(name);
    GFile *f = g_file_new_for_path(path);
    g_free(path);

    GError *err = NULL;
    GFileMonitor *m = g_file_monitor_file(f, G_FILE_MONITOR_NONE, NULL, &err);
    g_object_unref(f);

    if (!m) {
        if (err) { g_warning("monitor failed: %s", err->message); g_error_free(err); }
        return;
    }

    g_signal_connect(m, "changed", cb, NULL);
    // keep monitor alive for process lifetime
}

// ---------------------------
// Hyprland event socket
// ---------------------------

static char* find_hypr_socket2_path(void) {
    const char *xdg = g_getenv("XDG_RUNTIME_DIR");
    if (!xdg) return NULL;

    const char *sig = g_getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (sig && *sig) {
        return g_strdup_printf("%s/hypr/%s/.socket2.sock", xdg, sig);
    }

    // Fallback: scan $XDG_RUNTIME_DIR/hypr/*/.socket2.sock
    gchar *hypr_dir = g_strdup_printf("%s/hypr", xdg);
    DIR *d = opendir(hypr_dir);
    if (!d) { g_free(hypr_dir); return NULL; }

    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (ent->d_name[0] == '.') continue;
        gchar *candidate = g_strdup_printf("%s/%s/.socket2.sock", hypr_dir, ent->d_name);
        if (g_file_test(candidate, G_FILE_TEST_EXISTS)) {
            closedir(d);
            g_free(hypr_dir);
            return candidate;
        }
        g_free(candidate);
    }

    closedir(d);
    g_free(hypr_dir);
    return NULL;
}

static gpointer hypr_event_thread(gpointer data) {
    (void)data;

    char *path = find_hypr_socket2_path();
    if (!path) {
        g_warning("Hyprland event socket path not found; using polling fallback");
        ensure_polling_fallback();
        return NULL;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        g_warning("socket() failed: %s; using polling fallback", g_strerror(errno));
        g_free(path);
        ensure_polling_fallback();
        return NULL;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    g_strlcpy(addr.sun_path, path, sizeof(addr.sun_path));

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        g_warning("connect(%s) failed: %s; using polling fallback", path, g_strerror(errno));
        close(fd);
        g_free(path);
        ensure_polling_fallback();
        return NULL;
    }

    g_free(path);

    FILE *f = fdopen(fd, "r");
    if (!f) {
        close(fd);
        ensure_polling_fallback();
        return NULL;
    }

    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        if (g_str_has_prefix(line, "openwindow") ||
            g_str_has_prefix(line, "closewindow") ||
            g_str_has_prefix(line, "movewindow") ||
            g_str_has_prefix(line, "activewindow") ||
            g_str_has_prefix(line, "fullscreen") ||
            g_str_has_prefix(line, "changefloatingmode")) {
            schedule_refresh();
        }
    }

    fclose(f);
    ensure_polling_fallback();
    return NULL;
}

// ---------------------------
// GTK app activate
// ---------------------------

static void force_window_full_width(GtkWindow *win) {
    GdkDisplay *dpy = gdk_display_get_default();
    if (!dpy) return;

    GListModel *mons = gdk_display_get_monitors(dpy);
    if (!mons || g_list_model_get_n_items(mons) == 0) return;

    GdkMonitor *mon = GDK_MONITOR(g_list_model_get_item(mons, 0)); // ref
    if (!mon) return;

    GdkRectangle geo;
    gdk_monitor_get_geometry(mon, &geo);
    gtk_window_set_default_size(win, geo.width, 1); // full width, minimal height
    g_object_unref(mon);
}

static void on_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;

    // CSS provider (single provider, reloadable)
    g_css = dock_css_provider_create_and_attach();

    // Window
    GtkWidget *win = gtk_application_window_new(app);
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE);

    // Layer shell (GTK4)
    gtk_layer_init_for_window(GTK_WINDOW(win));
    gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_auto_exclusive_zone_enable(GTK_WINDOW(win));
		force_window_full_width(GTK_WINDOW(win));

    gtk_widget_add_css_class(win, "dock-window");

    // Full-width parent with centered dock
    GtkWidget *outer = gtk_center_box_new();
    gtk_widget_set_hexpand(outer, TRUE);
		gtk_widget_set_halign(outer, GTK_ALIGN_FILL);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(box, "dock");
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_END);

    g_box = box;

    gtk_center_box_set_center_widget(GTK_CENTER_BOX(outer), box);
    gtk_window_set_child(GTK_WINDOW(win), outer);

    // State
    rebuild_items_array();
    rebuild_dock_from_config();

    // Live reload
    watch_user_file("style.css",  G_CALLBACK(on_style_file_changed));
    watch_user_file("config.ini", G_CALLBACK(on_config_file_changed));

    // Initial refresh + events
    refresh_running(NULL);
    g_thread_new("hypr-events", hypr_event_thread, NULL);

    gtk_widget_set_visible(win, TRUE);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("local.hyprdock", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);

    // Cleanup
    if (g_poll_id) g_source_remove(g_poll_id);
    if (g_items) g_ptr_array_free(g_items, TRUE);
    if (g_cfg) dock_config_free(g_cfg);
    if (g_css) g_object_unref(g_css);
    g_object_unref(app);

    return status;
}


#include "dock.h"
#include "hypr.h"
#include "config.h"
#include "desktop_match.h"
#include "launcher.h"

#include <gio-unix-2.0/gio/gdesktopappinfo.h>

static void dock_item_free(gpointer p) {
    DockItem *it = (DockItem*)p;
    if (!it) return;
    g_free(it->desktop_id);
    g_free(it->match_key);
    g_free(it);
}

static void rebuild_items_array(HyprdockState *st) {
    if (st->items) g_ptr_array_free(st->items, TRUE);
    st->items = g_ptr_array_new_with_free_func(dock_item_free);
}

static void clear_box(GtkWidget *box) {
    GtkWidget *child = gtk_widget_get_first_child(box);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(box), child);
        child = next;
    }
}

gboolean idle_rebuild_config(gpointer data) {
    HyprdockState *st = (HyprdockState*)data;
    rebuild_dock_from_config(st);
    return G_SOURCE_REMOVE;
}

static GtkWidget* make_dot(void) {
    GtkWidget *d = gtk_frame_new(NULL);
    gtk_widget_add_css_class(d, "indicator");
    gtk_widget_set_size_request(d, 5, 9);
    gtk_widget_set_halign(d, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(d, GTK_ALIGN_END);
    gtk_widget_set_visible(d, FALSE);
    return d;
}

static GtkWidget* make_app_widget(HyprdockState *st, const char *desktop_id, int icon_size) {
    GtkWidget *btn = gtk_button_new();
    gtk_button_set_has_frame(GTK_BUTTON(btn), FALSE);
    gtk_widget_add_css_class(btn, "icon");

    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign(v, GTK_ALIGN_CENTER);

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

    GtkWidget *dot = make_dot();

    gtk_box_append(GTK_BOX(v), img);
    gtk_box_append(GTK_BOX(v), dot);
    gtk_button_set_child(GTK_BUTTON(btn), v);

    // click -> launch (user_data is strdup'd desktop_id)
    g_signal_connect_data(
        btn, "clicked",
        G_CALLBACK(on_app_clicked),
        g_strdup(desktop_id),
        (GClosureNotify)g_free,
        0
    );

    // track dot visibility for running refresh
    DockItem *it = g_new0(DockItem, 1);
    it->desktop_id = g_strdup(desktop_id);
    it->match_key  = desktop_match_key(desktop_id);
    it->dot        = dot;
    g_ptr_array_add(st->items, it);

    return btn;
}

static void dock_build_from_cfg(HyprdockState *st, const DockConfig *cfg) {
    clear_box(st->dock_box);
    rebuild_items_array(st);

    if (cfg && cfg->pinned_apps) {
        for (gchar **p = cfg->pinned_apps; *p; p++) {
            if (**p == '\0') continue;
            GtkWidget *w = make_app_widget(st, *p, cfg->icon_size);
            gtk_box_append(GTK_BOX(st->dock_box), w);
        }
    }

    // Update indicators once after building
    dock_refresh_running(st);
}

gboolean dock_refresh_running(gpointer user_data) {
    HyprdockState *st = (HyprdockState*)user_data;
    if (!st) return G_SOURCE_REMOVE;

    if (!st->items || st->items->len == 0) return G_SOURCE_CONTINUE;

    GHashTable *counts = hypr_get_running_class_counts();

    for (guint i = 0; i < st->items->len; i++) {
        DockItem *it = g_ptr_array_index(st->items, i);
        gpointer v = g_hash_table_lookup(counts, it->match_key);
        int c = v ? GPOINTER_TO_INT(v) : 0;
        gtk_widget_set_visible(it->dot, (c > 0));
    }

    g_hash_table_destroy(counts);
    return G_SOURCE_CONTINUE;
}

void rebuild_dock_from_config(HyprdockState *st) {
    if (!st) return;

    DockConfig *newcfg = dock_config_load();

    // Build UI from new config first, then swap ownership
    dock_build_from_cfg(st, newcfg);

    if (st->cfg) dock_config_free(st->cfg);
    st->cfg = newcfg;
}

void dock_init(HyprdockState *st) {
    if (!st || !st->dock_box) return;

    // If state_new already loaded config, reuse it; otherwise load it here.
    if (!st->cfg) st->cfg = dock_config_load();

    // Build dock UI from current cfg without reloading it.
    dock_build_from_cfg(st, st->cfg);
}

void dock_shutdown(HyprdockState *st) {
    if (!st) return;

    // Optional: clear the UI container (GTK will usually tear down anyway).
    if (st->dock_box) clear_box(st->dock_box);

    // Free dock runtime list (DockItem*, match_key, desktop_id, dot pointers).
    if (st->items) {
        g_ptr_array_free(st->items, TRUE);
        st->items = NULL;
    }

    // Do NOT free st->cfg here if state.c owns it.
}

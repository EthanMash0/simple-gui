#include "app.h"

#include <gtk/gtk.h>
#include <glib.h>
#include <glib-object.h>
#include <glib-unix.h>
#include <signal.h>

#include "state.h"
#include "dock.h"
#include "hypr_events.h"
#include "watch.h" // watch_user_file, on_style_file_changed, on_config_file_changed
#include "searcher.h"

/* App Searcher */

static gboolean on_sigusr1(gpointer user_data) {
	AppState *st = (AppState *)user_data;
	searcher_toggle(st);
	return G_SOURCE_CONTINUE;
}

/* App Dock */

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

    gtk_center_box_set_center_widget(GTK_CENTER_BOX(outer), box);
    gtk_window_set_child(GTK_WINDOW(win), outer);

    // Create state and attach it to the window for automatic cleanup
    AppState *st = app_state_new(box);
    g_object_set_data_full(
        G_OBJECT(win),
        "app-state",
        st,
        (GDestroyNotify)app_state_free
    );

    // Build dock UI from current config/state
    dock_init(st);
		
		// Initialize searcher
		searcher_init(st);

		// Listen for SIGUSR1 to toggle searcher
		g_unix_signal_add(SIGUSR1, on_sigusr1, st);

    // Live reload (these should be updated to accept/pass st as user_data)
    watch_user_file("style.css",  G_CALLBACK(on_style_file_changed),  st);
    watch_user_file("config.ini", G_CALLBACK(on_config_file_changed), st);

    // Events (thread / fallback polling should schedule refreshes using st)
    hypr_events_start(st);

    gtk_window_present(GTK_WINDOW(win));
}

GtkApplication *app_new(void) {
    GtkApplication *app =
        gtk_application_new("com.app.dock.hyprland", G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    return app;
}

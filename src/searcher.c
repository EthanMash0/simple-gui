#include "state.h"
#include <gtk/gtk.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>
#include <gio/gio.h>
#include <gdk/gdkkeysyms.h>

// static void on_search_app_clicked(GtkButton *btn, gpointer user_data) {
//     AppState *st = (AppState *)user_data;
//     GAppInfo *info = g_object_get_data(G_OBJECT(btn), "app-info");
//
//     if (info) {
//         g_app_info_launch(info, NULL, NULL, NULL);
//         gtk_widget_set_visible(st->search_box, FALSE);
//     }
// }

static gboolean on_key_pressed(GtkEventControllerKey *controller,
                               guint keyval,
                               guint keycode,
                               GdkModifierType state,
                               gpointer user_data) {
    AppState *st = (AppState *)user_data;
    
    // Check for Escape key
    if (keyval == GDK_KEY_Escape) {
        gtk_widget_set_visible(st->search_box, FALSE);
        return TRUE; // Return TRUE to stop the event from propagating further
    }
    
    return FALSE; // Let other keys propagate (e.g., typing in the search bar)
}

static gboolean search_filter_func(GtkFlowBoxChild *child, gpointer user_data) {
    GtkSearchEntry *entry = GTK_SEARCH_ENTRY(user_data);
    const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
    
    if (!text || !*text) return TRUE; 

    GtkWidget *btn = gtk_flow_box_child_get_child(child);
    GAppInfo *info = g_object_get_data(G_OBJECT(btn), "app-info");
    if (!info) return FALSE;

    const char *name = g_app_info_get_name(info);
    const char *id = g_app_info_get_id(info);
    
    char *lower_text = g_ascii_strdown(text, -1);
    char *lower_name = name ? g_ascii_strdown(name, -1) : NULL;
    char *lower_id = id ? g_ascii_strdown(id, -1) : NULL;

    gboolean match = (lower_name && strstr(lower_name, lower_text)) ||
                     (lower_id && strstr(lower_id, lower_text));

    g_free(lower_text);
    g_free(lower_name);
    g_free(lower_id);
    return match;
}

static void on_search_changed(GtkSearchEntry *entry, gpointer user_data) {
    GtkFlowBox *box = GTK_FLOW_BOX(user_data);
    gtk_flow_box_invalidate_filter(box);
}

void searcher_init(AppState *st) {
    GtkWidget *win = gtk_window_new();
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    gtk_widget_add_css_class(win, "search-window");

    // --- KEY CONTROLLER SETUP ---
    GtkEventController *key_controller = gtk_event_controller_key_new();
    // CRITICAL: Catch the event in the CAPTURE phase, before the SearchEntry sees it.
    gtk_event_controller_set_propagation_phase(key_controller, GTK_PHASE_CAPTURE);
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_key_pressed), st);
    gtk_widget_add_controller(win, key_controller);
    // ----------------------------

    gtk_layer_init_for_window(GTK_WINDOW(win));
    gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(win), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
    
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
		gtk_widget_add_css_class(box, "search-container");
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(box, 600, 400);
    gtk_window_set_child(GTK_WINDOW(win), box);

    GtkWidget *entry = gtk_search_entry_new();
    gtk_box_append(GTK_BOX(box), entry);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(box), scroll);

    GtkWidget *flow = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 5);
		gtk_widget_set_halign(flow, GTK_ALIGN_CENTER);
		gtk_widget_set_valign(flow, GTK_ALIGN_START);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), flow);

    GList *apps = g_app_info_get_all();
    for (GList *l = apps; l != NULL; l = l->next) {
        GAppInfo *info = (GAppInfo*)l->data;
        if (!g_app_info_should_show(info)) continue;

        GtkWidget *btn = gtk_button_new();
				gtk_widget_add_css_class(btn, "app-btn");
				gtk_widget_set_halign(btn, GTK_ALIGN_CENTER);
				gtk_widget_set_valign(btn, GTK_ALIGN_CENTER);
        GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
				gtk_widget_set_halign(vbox, GTK_ALIGN_CENTER);
				gtk_widget_set_valign(vbox, GTK_ALIGN_END);
        GIcon *icon = g_app_info_get_icon(info);
        
        GtkWidget *img = gtk_image_new_from_gicon(icon);
        gtk_image_set_pixel_size(GTK_IMAGE(img), st->cfg->searcher_icon_size);
        GtkWidget *lbl = gtk_label_new(g_app_info_get_name(info));
        gtk_label_set_wrap(GTK_LABEL(lbl), TRUE);
        gtk_label_set_max_width_chars(GTK_LABEL(lbl), 12);

        gtk_box_append(GTK_BOX(vbox), img);
        gtk_box_append(GTK_BOX(vbox), lbl);
        gtk_button_set_child(GTK_BUTTON(btn), vbox);

        g_object_set_data_full(G_OBJECT(btn), "app-info", g_object_ref(info), g_object_unref);
        // g_signal_connect(btn, "clicked", G_CALLBACK(on_search_app_clicked), st);
        gtk_flow_box_append(GTK_FLOW_BOX(flow), btn);
    }
    g_list_free_full(apps, g_object_unref);

    gtk_flow_box_set_filter_func(GTK_FLOW_BOX(flow), search_filter_func, entry, NULL);
    g_signal_connect(entry, "search-changed", G_CALLBACK(on_search_changed), flow);

    st->search_box = win;
    gtk_widget_set_visible(win, FALSE);
}

void searcher_toggle(AppState *st) {
    if (!st || !st->search_box) return;
    
    gboolean visible = gtk_widget_get_visible(st->search_box);
    
    if (visible) {
        gtk_widget_set_visible(st->search_box, FALSE);
    } else {
        gtk_widget_set_visible(st->search_box, TRUE);
        gtk_window_present(GTK_WINDOW(st->search_box));
    }
}

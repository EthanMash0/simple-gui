#ifndef DOCK_H
#define DOCK_H

#include <gtk/gtk.h>
#include "state.h"

typedef struct {
    char *desktop_id;   // e.g. "firefox.desktop"
    char *match_key;    // lowercased StartupWMClass or desktop-id fallback
    GtkWidget *dot;     // indicator widget
} DockItem;

// typedef struct HyprdockState HyprdockState;

gboolean idle_rebuild_config(gpointer data);

gboolean dock_refresh_running(gpointer user_data);

void rebuild_dock_from_config(HyprdockState *st);

// Called once after you create the dock box in app.c
void dock_init(HyprdockState *st);

void dock_shutdown(HyprdockState *st);

#endif

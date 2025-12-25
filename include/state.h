#ifndef STATE_H
#define STATE_H

#include "config.h"
#include "gtk/gtkshortcut.h"

typedef struct {
  GtkWidget *dock_box;
  GtkCssProvider *css;

  DockConfig *cfg;

  GPtrArray *items;        // DockItem*
  guint poll_id;           // polling fallback (if you keep it here)

  gint refresh_pending;    // atomic coalesce flag
  GThread *event_thread;   // optional if you want to track it
	GPtrArray *monitors; // (GFileMonitor*) free func g_object_unref
	
	int event_fd;						// -1 if none
	gint stop_requested;		// atomic
	guint refresh_idle_id;	// recommended so stop can cancel it
} HyprdockState;

HyprdockState *hyprdock_state_new(GtkWidget *dock_box);
void hyprdock_state_free(HyprdockState *st);

#endif

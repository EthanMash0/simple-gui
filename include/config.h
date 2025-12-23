#ifndef CONFIG_H
#define CONFIG_H

#include <glib-2.0/glib.h>
#include <gtk-4.0/gtk/gtk.h>

typedef struct {
	gchar **pinned_apps;
	int icon_size;
} DockConfig;

DockConfig* dock_config_load(void);
void dock_config_free(DockConfig *cfg);

gchar* dock_find_config_path(const char *name);
gchar* dock_user_config_path(const char *name);

// void dock_apply_css(void);
GtkCssProvider* dock_css_provider_create_and_attach(void);
gboolean dock_css_provider_reload(GtkCssProvider *prov);

#endif

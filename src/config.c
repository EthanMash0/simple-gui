#include "config.h"
#include <gtk-4.0/gtk/gtk.h>
#include <glib-2.0/glib.h>

gchar* dock_user_config_path(const char *name) {
	const char *xdg = g_get_user_config_dir();
	return g_build_filename(xdg, "hyprdock", name, NULL);
}

gchar* dock_find_config_path(const char *name) {
	gchar *user = dock_user_config_path(name);
	if (g_file_test(user, G_FILE_TEST_EXISTS)) return user;

	g_free(user);
	return g_build_filename("/usr/share/hyprdock", name, NULL);
}

static gchar** split_csv_trim(const gchar *s) {
	if (!s || !*s) return NULL;

	gchar **v = g_strsplit(s, ",", -1);
	for (char **p = v; p && *p; p++) g_strstrip(*p);

	gboolean any = FALSE;
	for (gchar **p = v; p && *p; p++) {
		if (**p != '\0') {
			any = TRUE;
			break;
		}
	}
	if (!any) {
		g_strfreev(v);
		return NULL;
	}

	return v;
}

DockConfig* dock_config_load(void) {
	DockConfig *cfg = g_new0(DockConfig, 1);
	cfg->icon_size = 32;

	GKeyFile *kf = g_key_file_new();
	gchar *path = dock_find_config_path("config.ini");

	if (!g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL)) {
		g_warning("Failed to load config: %s", path);
		g_free(path);
		g_key_file_free(kf);
		return cfg;
	}
	g_free(path);

	GError *err = NULL;
	int icon_size = g_key_file_get_integer(kf, "dock", "icon_size", &err);
	if (!err && icon_size > 0 && icon_size <= 256) cfg->icon_size = icon_size;
	if (err) g_error_free(err);

	gchar *apps = g_key_file_get_string(kf, "pinned", "apps", NULL);
	cfg->pinned_apps = split_csv_trim(apps);
	g_free(apps);

	g_key_file_free(kf);
	return cfg;
}

void dock_config_free(DockConfig *cfg) {
	if (!cfg) return;
	g_strfreev(cfg->pinned_apps);
	g_free(cfg);
}

GtkCssProvider* dock_css_provider_create_and_attach(void) {
	GtkCssProvider *prov = gtk_css_provider_new();
	dock_css_provider_reload(prov);

	gtk_style_context_add_provider_for_display(
		gdk_display_get_default(),
		GTK_STYLE_PROVIDER(prov),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
	);
	return prov;
}

gboolean dock_css_provider_reload(GtkCssProvider *prov) {
	// gchar *path = dock_find_config_path("style.css");
	gchar *path = dock_user_config_path("style.css");
	gboolean ok = g_file_test(path, G_FILE_TEST_EXISTS);
	if (!ok) {
		g_warning("CSS not found: %s", path);
		g_free(path);
		return FALSE;
	}

	gtk_css_provider_load_from_path(prov, path);
	g_free(path);
	return TRUE;
}

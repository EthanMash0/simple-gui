#ifndef WATCH_H
#define WATCH_H

#include <gtk4-layer-shell/gtk4-layer-shell.h>

void on_config_file_changed(
		GFileMonitor *mon, 
		GFile *file, 
		GFile *other, 
		GFileMonitorEvent ev, 
		gpointer user_data);

void on_style_file_changed(
		GFileMonitor *mon, 
		GFile *file, 
		GFile *other, 
		GFileMonitorEvent ev, 
		gpointer user_data);

void watch_user_file(const char *name, GCallback cb, gpointer user_data);

#endif

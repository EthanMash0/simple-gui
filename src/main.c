// #define _GNU_SOURCE
#include "app.h"

int main(int argc, char **argv) {
    GtkApplication *app = hyprdock_app_new();
    int status = g_application_run(G_APPLICATION(app), argc, argv);
		g_object_unref(app);
    return status;
}


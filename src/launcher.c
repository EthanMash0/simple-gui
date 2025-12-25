#include "launcher.h"

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <gio-unix-2.0/gio/gdesktopappinfo.h>
#include <glib.h>
#include <string.h>

static char *strip_desktop_field_codes(const char *exec) {
    // Removes %f %F %u %U %i %c %k etc. Keeps literal %% as %
    GString *out = g_string_new(NULL);
    for (const char *p = exec; *p; p++) {
        if (*p != '%') {
            g_string_append_c(out, *p);
            continue;
        }

        char n = *(p + 1);
        if (!n) break;

        if (n == '%') { // literal %
            g_string_append_c(out, '%');
            p++;
            continue;
        }

        // skip known field codes (single char after %)
        // (covers common spec codes; harmless if we skip extras)
        if (strchr("fFuUdDnNickvm", n) != NULL) {
            p++; // skip the code char too
            continue;
        }

        // Unknown pattern: keep it as-is
        g_string_append_c(out, *p);
    }
    return g_string_free(out, FALSE);
}

typedef struct {
    const char *exe;
    const char *inject[3]; // tokens inserted before the command argv
} TermSpec;

static const TermSpec *term_spec_for(const char *exe_base) {
    static const TermSpec specs[] = {
        { "foot",          { "-e", NULL, NULL } },
        { "alacritty",     { "-e", NULL, NULL } },
        { "kitty",         { "--", NULL, NULL } },
        { "wezterm",       { "start", "--", NULL } },
        { "gnome-terminal",{ "--", NULL, NULL } },
        { "konsole",       { "-e", NULL, NULL } },
        { "xterm",         { "-e", NULL, NULL } },
    };

    for (guint i = 0; i < G_N_ELEMENTS(specs); i++) {
        if (g_strcmp0(exe_base, specs[i].exe) == 0) return &specs[i];
    }
    return NULL;
}

static char **build_terminal_argv(char **cmd_argv) {
    // Allow user override:
    //   HYPRDOCK_TERMINAL (preferred) or TERMINAL
    const char *env = g_getenv("HYPRDOCK_TERMINAL");
    if (!env || !*env) env = g_getenv("TERMINAL");

    char **term_argv = NULL;
    int term_argc = 0;
    GError *err = NULL;

    if (env && *env) {
        if (!g_shell_parse_argv(env, &term_argc, &term_argv, &err)) {
            g_warning("failed to parse terminal env '%s': %s", env, err->message);
            g_clear_error(&err);
            g_clear_pointer(&term_argv, g_strfreev);
            term_argc = 0;
        }
    }

    // If no env, pick the first terminal found in PATH
    if (!term_argv) {
        const char *candidates[] = {
            "foot", "alacritty", "kitty", "wezterm", "gnome-terminal", "konsole", "xterm"
        };
        for (guint i = 0; i < G_N_ELEMENTS(candidates); i++) {
            if (g_find_program_in_path(candidates[i])) {
                term_argv = g_new0(char*, 2);
                term_argv[0] = g_strdup(candidates[i]);
                term_argc = 1;
                break;
            }
        }
    }

    if (!term_argv) return NULL;

    char *base = g_path_get_basename(term_argv[0]);
    const TermSpec *spec = term_spec_for(base);
    g_free(base);

    // Default injection if unknown: try "-e"
    const char *inj0 = spec ? spec->inject[0] : "-e";
    const char *inj1 = spec ? spec->inject[1] : NULL;

    // Count cmd argv
    int cmd_argc = 0;
    while (cmd_argv && cmd_argv[cmd_argc]) cmd_argc++;

    int outc = term_argc + (inj0 ? 1 : 0) + (inj1 ? 1 : 0) + cmd_argc;
    char **outv = g_new0(char*, outc + 1);

    int k = 0;
    for (int i = 0; i < term_argc; i++) outv[k++] = g_strdup(term_argv[i]);
    if (inj0) outv[k++] = g_strdup(inj0);
    if (inj1) outv[k++] = g_strdup(inj1);
    for (int i = 0; i < cmd_argc; i++) outv[k++] = g_strdup(cmd_argv[i]);
    outv[k] = NULL;

    g_strfreev(term_argv);
    return outv;
}

void on_app_clicked(GtkButton *b, gpointer user_data) {
    (void)b;
    const char *desktop_id = (const char*)user_data;

    GDesktopAppInfo *app = g_desktop_app_info_new(desktop_id);
    if (!app) {
        g_warning("No desktop entry found: %s", desktop_id);
        return;
    }

    gboolean terminal = g_desktop_app_info_get_boolean(app, "Terminal");

    if (!terminal) {
        GError *err = NULL;
        if (!g_app_info_launch(G_APP_INFO(app), NULL, NULL, &err)) {
            if (err) {
                g_warning("launch failed for %s: %s", desktop_id, err->message);
                g_error_free(err);
            }
        }
        g_object_unref(app);
        return;
    }

    // Terminal=true: manually spawn inside a terminal emulator
    const char *exec = g_desktop_app_info_get_string(app, "Exec");
    if (!exec || !*exec) {
        g_warning("desktop entry %s has Terminal=true but no Exec", desktop_id);
        g_object_unref(app);
        return;
    }

    char *clean = strip_desktop_field_codes(exec);

    int argc = 0;
    char **argv = NULL;
    GError *err = NULL;
    if (!g_shell_parse_argv(clean, &argc, &argv, &err)) {
        g_warning("failed to parse Exec for %s: %s", desktop_id, err->message);
        g_error_free(err);
        g_free(clean);
        g_object_unref(app);
        return;
    }
    g_free(clean);

    char **targv = build_terminal_argv(argv);
    g_strfreev(argv);

    if (!targv) {
        g_warning("Terminal=true for %s, but no terminal emulator found", desktop_id);
        g_object_unref(app);
        return;
    }

    if (!g_spawn_async(NULL, targv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &err)) {
        g_warning("terminal launch failed for %s: %s", desktop_id, err->message);
        g_error_free(err);
    }

    g_strfreev(targv);
    g_object_unref(app);
}

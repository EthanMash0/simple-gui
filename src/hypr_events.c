#include "hypr_events.h"

#include <glib.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "state.h"
#include "dock.h"

static gboolean add_poll_source_cb(gpointer data) {
    HyprdockState *st = data;
    if (!st) return G_SOURCE_REMOVE;

    if (st->poll_id == 0) {
        st->poll_id = g_timeout_add_seconds(1, (GSourceFunc)dock_refresh_running, st);
    }
    return G_SOURCE_REMOVE;
}

static void ensure_polling_fallback(HyprdockState *st) {
    if (!st) return;
    // Always add sources on the main context.
    g_main_context_invoke(NULL, add_poll_source_cb, st);
}

static gboolean refresh_idle_cb(gpointer data) {
    HyprdockState *st = data;
    if (!st) return G_SOURCE_REMOVE;

    st->refresh_idle_id = 0;
    g_atomic_int_set(&st->refresh_pending, 0);

    if (!g_atomic_int_get(&st->stop_requested)) {
        dock_refresh_running(st);
    }
    return G_SOURCE_REMOVE;
}

static void schedule_refresh(HyprdockState *st) {
    if (!st) return;
    if (g_atomic_int_get(&st->stop_requested)) return;

    if (!g_atomic_int_compare_and_exchange(&st->refresh_pending, 0, 1)) return;

    // Track the idle id so stop() can remove it if needed.
    st->refresh_idle_id = g_idle_add((GSourceFunc)refresh_idle_cb, st);
}

static char *find_hypr_socket2_path(void) {
    const char *xdg = g_getenv("XDG_RUNTIME_DIR");
    if (!xdg) return NULL;

    const char *sig = g_getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (sig && *sig) {
        return g_strdup_printf("%s/hypr/%s/.socket2.sock", xdg, sig);
    }

    // Fallback: scan $XDG_RUNTIME_DIR/hypr/*/.socket2.sock
    gchar *hypr_dir = g_strdup_printf("%s/hypr", xdg);
    DIR *d = opendir(hypr_dir);
    if (!d) { g_free(hypr_dir); return NULL; }

    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (ent->d_name[0] == '.') continue;

        gchar *candidate = g_strdup_printf("%s/%s/.socket2.sock", hypr_dir, ent->d_name);
        if (g_file_test(candidate, G_FILE_TEST_EXISTS)) {
            closedir(d);
            g_free(hypr_dir);
            return candidate;
        }
        g_free(candidate);
    }

    closedir(d);
    g_free(hypr_dir);
    return NULL;
}

static gpointer hypr_event_thread(gpointer data) {
    HyprdockState *st = data;
    if (!st) return NULL;

    char *path = find_hypr_socket2_path();
    if (!path) {
        g_warning("Hyprland event socket path not found; using polling fallback");
        ensure_polling_fallback(st);
        return NULL;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        g_warning("socket() failed: %s; using polling fallback", g_strerror(errno));
        g_free(path);
        ensure_polling_fallback(st);
        return NULL;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    g_strlcpy(addr.sun_path, path, sizeof(addr.sun_path));

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        g_warning("connect(%s) failed: %s; using polling fallback", path, g_strerror(errno));
        close(fd);
        g_free(path);
        ensure_polling_fallback(st);
        return NULL;
    }

    // publish fd so stop() can interrupt the blocking read()
    st->event_fd = fd;

    g_message("Connected to Hyprland event socket: %s", path);
    g_free(path);

    char buf[4096];
    GString *acc = g_string_new(NULL);

    while (!g_atomic_int_get(&st->stop_requested)) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n <= 0) break;

        g_string_append_len(acc, buf, (gssize)n);

        for (;;) {
            char *nl = strchr(acc->str, '\n');
            if (!nl) break;

            // Any event -> refresh (coalesced)
            schedule_refresh(st);

            gsize linelen = (gsize)(nl - acc->str);
            g_string_erase(acc, 0, linelen + 1);
        }
    }

    g_string_free(acc, TRUE);

    // Close fd here (thread owns it)
    close(fd);
    st->event_fd = -1;

    if (!g_atomic_int_get(&st->stop_requested)) {
        g_warning("Hyprland event socket disconnected; using polling fallback");
        ensure_polling_fallback(st);
    }

    return NULL;
}

void hypr_events_start(HyprdockState *st) {
    if (!st) return;

    if (st->event_thread) return; // already started

    g_atomic_int_set(&st->stop_requested, 0);
    st->event_fd = -1;

    st->event_thread = g_thread_new("hypr-events", hypr_event_thread, st);
}

void hypr_events_stop(HyprdockState *st) {
    if (!st) return;

    g_atomic_int_set(&st->stop_requested, 1);

    // Cancel polling fallback timer if present
    if (st->poll_id) {
        g_source_remove(st->poll_id);
        st->poll_id = 0;
    }

    // Cancel a queued idle refresh if present
    if (st->refresh_idle_id) {
        g_source_remove(st->refresh_idle_id);
        st->refresh_idle_id = 0;
        g_atomic_int_set(&st->refresh_pending, 0);
    }

    // Interrupt blocking read()
    if (st->event_fd >= 0) {
        shutdown(st->event_fd, SHUT_RDWR);
        // do not close() here; thread closes it
    }

    // Join thread
    if (st->event_thread) {
        g_thread_join(st->event_thread);
        st->event_thread = NULL;
    }
}

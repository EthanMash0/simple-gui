#define _GNU_SOURCE
#include "hypr.h"
#include "jsmn.h"
#include <glib-2.0/glib.h>
#include <stdio.h>
#include <string.h>

static char* read_cmd_all(const char *cmd) {
	FILE *fp = popen(cmd, "r");
	if (!fp) return NULL;

	GString *buf = g_string_new(NULL);
	char tmp[4096];
	while (fgets(tmp, sizeof(tmp), fp)) g_string_append(buf, tmp);

	int rc = pclose(fp);
	(void)rc;

	return g_string_free(buf, FALSE);
}

static gboolean token_streq(const char *json, const jsmntok_t *t, const char *s) {
	int len = t->end - t->start;
	return (
			t->type == JSMN_STRING &&
			(int)strlen(s) == len &&
			strncmp(json + t->start, s, len) == 0
	);
}

static char* token_strdup(const char *json, const jsmntok_t *t) {
	int len = t->end - t->start;
	return g_strndup(json + t->start, len);
}

GHashTable* hypr_get_running_class_counts(void) {
    char *json = read_cmd_all("hyprctl -j clients");
    GHashTable *m = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    if (!json) return m;

    jsmn_parser p;
    jsmn_init(&p);

    int cap = 8192;
    jsmntok_t *tok = g_new0(jsmntok_t, cap);
    int n = jsmn_parse(&p, json, (int)strlen(json), tok, cap);

    if (n >= 0) {
        for (int i = 0; i < n - 1; i++) {
            if (token_streq(json, &tok[i], "class") && tok[i+1].type == JSMN_STRING) {
                char *cls = token_strdup(json, &tok[i+1]);
                g_strstrip(cls);

                char *lower = g_ascii_strdown(cls, -1);
                g_free(cls);

                if (lower && *lower) {
                    gpointer oldv = g_hash_table_lookup(m, lower);
                    int oldc = oldv ? GPOINTER_TO_INT(oldv) : 0;
                    g_hash_table_insert(m, lower, GINT_TO_POINTER(oldc + 1));
                } else {
                    g_free(lower);
                }
                i++; // skip value token
            }
        }
    }

    g_free(tok);
    g_free(json);
    return m;
}

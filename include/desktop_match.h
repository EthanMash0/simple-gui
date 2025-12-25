#ifndef DESKTOP_MATCH_H
#define DESKTOP_MATCH_H

// G_BEGIN_DECLS

// Returns a lowercase key used to match Hyprland "class" to a desktop entry.
// Caller owns the returned string (free with g_free()).
char *desktop_match_key(const char *desktop_id);

// G_END_DECLS

#endif

/*
 * jsmn.h
 * Minimalistic JSON parser in C.
 *
 * This is the single-header form (includes implementation).
 * Based on jsmn by Serge Zaitsev (MIT license).
 */
#ifndef JSMN_H
#define JSMN_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  JSMN_UNDEFINED = 0,
  JSMN_OBJECT = 1,
  JSMN_ARRAY = 2,
  JSMN_STRING = 3,
  JSMN_PRIMITIVE = 4
} jsmntype_t;

enum jsmnerr {
  JSMN_ERROR_NOMEM = -1,
  JSMN_ERROR_INVAL = -2,
  JSMN_ERROR_PART  = -3
};

typedef struct {
  jsmntype_t type;
  int start;
  int end;
  int size;
#ifdef JSMN_PARENT_LINKS
  int parent;
#endif
} jsmntok_t;

typedef struct {
  unsigned int pos;     /* offset in the JSON string */
  unsigned int toknext; /* next token to allocate */
  int toksuper;         /* superior token node, e.g. parent object or array */
} jsmn_parser;

static void jsmn_init(jsmn_parser *parser) {
  parser->pos = 0;
  parser->toknext = 0;
  parser->toksuper = -1;
}

static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser, jsmntok_t *tokens, size_t num_tokens) {
  if (parser->toknext >= num_tokens) return NULL;
  jsmntok_t *tok = &tokens[parser->toknext++];
  tok->start = tok->end = -1;
  tok->size = 0;
#ifdef JSMN_PARENT_LINKS
  tok->parent = -1;
#endif
  tok->type = JSMN_UNDEFINED;
  return tok;
}

static void jsmn_fill_token(jsmntok_t *token, jsmntype_t type, int start, int end) {
  token->type = type;
  token->start = start;
  token->end = end;
  token->size = 0;
}

static int jsmn_parse_primitive(jsmn_parser *parser, const char *js, size_t len,
                                jsmntok_t *tokens, size_t num_tokens) {
  int start = (int)parser->pos;

  for (; parser->pos < len; parser->pos++) {
    char c = js[parser->pos];
    if (c == '\t' || c == '\r' || c == '\n' || c == ' ' ||
        c == ','  || c == ']'  || c == '}') {
      break;
    }
    if (c < 32 || c == '"' || c == '\\') {
      parser->pos = (unsigned int)start;
      return JSMN_ERROR_INVAL;
    }
  }

  jsmntok_t *tok = jsmn_alloc_token(parser, tokens, num_tokens);
  if (!tok) {
    parser->pos = (unsigned int)start;
    return JSMN_ERROR_NOMEM;
  }
  jsmn_fill_token(tok, JSMN_PRIMITIVE, start, (int)parser->pos);
#ifdef JSMN_PARENT_LINKS
  tok->parent = parser->toksuper;
#endif
  parser->pos--;
  return 0;
}

static int jsmn_parse_string(jsmn_parser *parser, const char *js, size_t len,
                             jsmntok_t *tokens, size_t num_tokens) {
  int start = (int)parser->pos;
  parser->pos++;

  for (; parser->pos < len; parser->pos++) {
    char c = js[parser->pos];

    if (c == '"') {
      jsmntok_t *tok = jsmn_alloc_token(parser, tokens, num_tokens);
      if (!tok) {
        parser->pos = (unsigned int)start;
        return JSMN_ERROR_NOMEM;
      }
      jsmn_fill_token(tok, JSMN_STRING, start + 1, (int)parser->pos);
#ifdef JSMN_PARENT_LINKS
      tok->parent = parser->toksuper;
#endif
      return 0;
    }

    if (c == '\\') {
      parser->pos++;
      if (parser->pos >= len) {
        parser->pos = (unsigned int)start;
        return JSMN_ERROR_PART;
      }
      c = js[parser->pos];
      switch (c) {
        case '"': case '/': case '\\':
        case 'b': case 'f': case 'r':
        case 'n': case 't':
          break;
        case 'u':
          /* skip \uXXXX */
          for (int i = 0; i < 4; i++) {
            parser->pos++;
            if (parser->pos >= len) {
              parser->pos = (unsigned int)start;
              return JSMN_ERROR_PART;
            }
            char hc = js[parser->pos];
            if (!((hc >= '0' && hc <= '9') ||
                  (hc >= 'A' && hc <= 'F') ||
                  (hc >= 'a' && hc <= 'f'))) {
              parser->pos = (unsigned int)start;
              return JSMN_ERROR_INVAL;
            }
          }
          break;
        default:
          parser->pos = (unsigned int)start;
          return JSMN_ERROR_INVAL;
      }
    }
  }

  parser->pos = (unsigned int)start;
  return JSMN_ERROR_PART;
}

static int jsmn_parse(jsmn_parser *parser, const char *js, size_t len,
                      jsmntok_t *tokens, unsigned int num_tokens) {
  int r;
  int count = (int)parser->toknext;

  for (; parser->pos < len; parser->pos++) {
    char c = js[parser->pos];

    switch (c) {
      case '{': case '[': {
        jsmntok_t *tok = jsmn_alloc_token(parser, tokens, num_tokens);
        if (!tok) return JSMN_ERROR_NOMEM;
        tok->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
        tok->start = (int)parser->pos;
#ifdef JSMN_PARENT_LINKS
        tok->parent = parser->toksuper;
#endif
        if (parser->toksuper != -1) tokens[parser->toksuper].size++;
        parser->toksuper = (int)(parser->toknext - 1);
        count++;
        break;
      }

      case '}': case ']': {
        jsmntype_t type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
        int i = (int)parser->toknext - 1;
        for (; i >= 0; i--) {
          jsmntok_t *tok = &tokens[i];
          if (tok->start != -1 && tok->end == -1) {
            if (tok->type != type) return JSMN_ERROR_INVAL;
            tok->end = (int)parser->pos + 1;
            parser->toksuper =
#ifdef JSMN_PARENT_LINKS
              tok->parent;
#else
              -1;
              for (int j = i - 1; j >= 0; j--) {
                if (tokens[j].start != -1 && tokens[j].end == -1) {
                  parser->toksuper = j;
                  break;
                }
              }
#endif
            break;
          }
        }
        if (i == -1) return JSMN_ERROR_INVAL;
        break;
      }

      case '"':
        r = jsmn_parse_string(parser, js, len, tokens, num_tokens);
        if (r < 0) return r;
        count++;
        if (parser->toksuper != -1) tokens[parser->toksuper].size++;
        break;

      case '\t': case '\r': case '\n': case ' ':
      case ':': case ',':
        break;

      default:
        r = jsmn_parse_primitive(parser, js, len, tokens, num_tokens);
        if (r < 0) return r;
        count++;
        if (parser->toksuper != -1) tokens[parser->toksuper].size++;
        break;
    }
  }

  for (unsigned int i = 0; i < parser->toknext; i++) {
    if (tokens[i].start != -1 && tokens[i].end == -1) return JSMN_ERROR_PART;
  }

  return count;
}

#ifdef __cplusplus
}
#endif

#endif /* JSMN_H */

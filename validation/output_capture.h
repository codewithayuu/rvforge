#ifndef RVFORGE_OUTPUT_CAPTURE_H
#define RVFORGE_OUTPUT_CAPTURE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define CAPTURE_BUF_SIZE (1024 * 1024)

typedef struct {
    char   *buf;
    size_t  cap;
    size_t  len;
    int     active;
} OutputCapture;

static OutputCapture g_capture = {NULL, 0, 0, 0};

static inline int capture_start(OutputCapture *c)
{
    c->buf = (char *)malloc(CAPTURE_BUF_SIZE);
    if (!c->buf) return -1;
    c->cap    = CAPTURE_BUF_SIZE;
    c->len    = 0;
    c->active = 1;
    c->buf[0] = '\0';
    return 0;
}

static inline void capture_stop(OutputCapture *c)
{
    c->active = 0;
}

static inline void capture_free(OutputCapture *c)
{
    free(c->buf);
    c->buf    = NULL;
    c->len    = 0;
    c->cap    = 0;
    c->active = 0;
}

static inline int capture_append(OutputCapture *c, const char *fmt, ...)
{
    if (!c->active) return 0;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(c->buf + c->len, c->cap - c->len, fmt, ap);
    va_end(ap);
    if (n > 0) c->len += (size_t)n;
    return n;
}

static inline int capture_write_file(const OutputCapture *c, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fwrite(c->buf, 1, c->len, f);
    fclose(f);
    return 0;
}

static inline char *capture_find_line(OutputCapture *c, const char *prefix)
{
    char *p = c->buf;
    size_t plen = strlen(prefix);
    while (p < c->buf + c->len) {
        if (strncmp(p, prefix, plen) == 0) return p;
        char *nl = strchr(p, '\n');
        if (!nl) break;
        p = nl + 1;
    }
    return NULL;
}

static inline double capture_extract_double(OutputCapture *c,
                                            const char *prefix,
                                            const char *key)
{
    char *line = capture_find_line(c, prefix);
    if (!line) return 0.0;
    char *k = strstr(line, key);
    if (!k) return 0.0;
    k += strlen(key);
    return strtod(k, NULL);
}

static inline int capture_count_pattern(const OutputCapture *c,
                                        const char *pattern)
{
    int count  = 0;
    const char *p = c->buf;
    size_t plen   = strlen(pattern);
    while ((p = strstr(p, pattern)) != NULL) {
        count++;
        p += plen;
    }
    return count;
}

#endif /* RVFORGE_OUTPUT_CAPTURE_H */

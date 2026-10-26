/* alloc_audit.c — implementation of the allocation ledger.
 *
 * ⚠ COMPILED WITHOUT the `-include src/alloc_audit.h` flag (see Makefile):
 * plain CFLAGS, so the macros in alloc_audit.h are NOT active in this TU and
 * the kernel calls below hit the REAL malloc/free. If you compile this file
 * with the -include flag the wrappers wrap themselves and stack-overflow —
 * that failure IS the design: the flag belongs in build rules, not sources.
 *
 * Storage of the ledger itself uses the allocator directly but is invisible
 * in the report (entries are removed when their tracked block is freed). */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* same signatures as alloc_audit.h (duplicated so this TU needs no macros) */void  *audit_malloc (int line, size_t size);
void  *audit_calloc (int line, size_t n, size_t size);
void  *audit_realloc(int line, void *p, size_t size);
char  *audit_strdup (int line, const char *s);
void   audit_free   (int line, void *p);
void   audit_report (void);

typedef struct AEnt {
    void          *p;
    size_t         sz;
    int            line;
    struct AEnt   *next;
} AEnt;

static AEnt  *g_live;        /* LIFO list of live blocks */
static size_t g_live_cnt;    /* live block count */
static size_t g_live_bytes;  /* live bytes */
static size_t g_peak;
static size_t g_total;       /* every allocation ever seen */
static size_t g_bad_frees;   /* free() of a pointer we never handed out */
static int    g_attached;    /* atexit registered? */

static void ledger_add(void *p, size_t sz, int line) {
    AEnt *e;
    if (!p) return;                    /* NULL allocation: handler */
    if (!g_attached) {                 /* report automatically at exit */
        g_attached = 1;
        atexit(audit_report);
    }
    e = (AEnt *)malloc(sizeof *e);     /* real malloc (uninterposed TU) */
    if (!e) {
        fprintf(stderr, "ALLOC-AUDIT: out of ledger memory\n");
        return;
    }
    e->p = p; e->sz = sz; e->line = line;
    e->next = g_live;
    g_live = e;
    g_live_cnt++;
    g_live_bytes += sz;
    if (g_live_cnt > g_peak) g_peak = g_live_cnt;
}

/* removes the entry whose ->p == p; returns the size it was tracking */
static int ledger_del(void *p, size_t *sz_out, int *line_out) {
    AEnt **pp = &g_live;
    for (; *pp; pp = &(*pp)->next) {
        if ((*pp)->p == p) {
            AEnt *dead = *pp;
            size_t dead_sz  = dead->sz;
            int    dead_line = dead->line;
            *pp = dead->next;
            if (sz_out)   *sz_out   = dead_sz;
            if (line_out) *line_out = dead_line;
            g_live_cnt--;
            g_live_bytes -= dead_sz;
            free(dead);                         /* real free */
            return 1;
        }
    }
    return 0;
}

/* attach-on-first-alloc is inlined into ledger_add() above */

void *audit_malloc(int line, size_t size) {
    void *p = malloc(size);              /* REAL */
    ledger_add(p, size, line);
    g_total++;
    return p;
}

void *audit_calloc(int line, size_t n, size_t size) {
    void *p = calloc(n, size);           /* REAL */
    ledger_add(p, n * size, line);
    g_total++;
    return p;
}

char *audit_strdup(int line, const char *s) {
    size_t len;
    char *d;
    if (!s) return NULL;
    len = strlen(s) + 1;
    d = (char *)malloc(len);             /* REAL */
    if (d) memcpy(d, s, len);
    ledger_add(d, len, line);
    g_total++;
    return d;
}

void *audit_realloc(int line, void *p, size_t size) {
    void *np = realloc(p, size);         /* REAL */
    size_t old_sz = 0;
    if (p && !ledger_del(p, &old_sz, NULL)) {
        /* realloc of a pointer not from our ledger (e.g. system lib) */
        fprintf(stderr, "ALLOC-AUDIT: realloc of foreign ptr %p (line %d)\n",
                p, line);
    }
    ledger_add(np, size, line);
    g_total++;
    return np;
}

void audit_free(int line, void *p) {
    int a_line = 0;
    if (!p) return;                      /* free(NULL): no-op, matches C */
    if (!ledger_del(p, NULL, &a_line)) {
        g_bad_frees++;
        fprintf(stderr, "ALLOC-AUDIT: free of unknown/foreign ptr %p "
                        "at line %d\n", p, line);
        return;                          /* do NOT pass to free() */
    }
    free(p);                             /* REAL, after ledger removal */
}

void audit_report(void) {
    AEnt *e;
    int shown = 0;
    fprintf(stderr,
        "ALLOC-AUDIT: total=%lu alloc(s), peak live=%lu, "
        "in-use at exit: %lu block(s), %lu byte(s), %lu bad free(s)\n",
        (unsigned long)g_total, (unsigned long)g_peak,
        (unsigned long)g_live_cnt, (unsigned long)g_live_bytes,
        (unsigned long)g_bad_frees);

    for (e = g_live; e && shown < 8; e = e->next, shown++)
        fprintf(stderr,
            "ALLOC-AUDIT LEAK: %lu byte(s) allocated by src line %d\n",
            (unsigned long)e->sz, e->line);

    if (g_live_cnt == 0 && g_bad_frees == 0)
        fprintf(stderr, "ALLOC-AUDIT VERDICT: CLEAN\n");
    else
        fprintf(stderr, "ALLOC-AUDIT VERDICT: LEAK (%lu block(s), %lu "
                        "byte(s), %lu bad frees) — see entries above\n",
                (unsigned long)g_live_cnt, (unsigned long)g_live_bytes,
                (unsigned long)g_bad_frees);
}

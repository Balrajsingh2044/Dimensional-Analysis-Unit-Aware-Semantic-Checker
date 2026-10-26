/* alloc_audit.h — portable allocation ledger (Balraj, final QA).
 *
 * NOT part of the frozen include/ contract — this lives in src/ on purpose.
 * It is compiled IN via the compiler-wide `-include src/alloc_audit.h` flag
 * (see the Makefile `audit` target), so it participates in nothing unless
 * explicitly asked for. Normal builds, the contract headers, the drivers and
 * the test sources are untouched.
 *
 * Purpose: Ayansh's valgrind baseline ask (docs/message_to_balraj.md) must be
 * answered on a box with NO valgrind and NO WSL. Every allocation that
 * reaches our code (our .c files AND the generated lexer / parser — flex
 * buffers, bison strdups, dim_strdup) is routed through
 * audit_malloc/calloc/realloc/strdup/free, which keep an address-keyed
 * ledger. At exit an automatic report prints:
 *
 *   ALLOC-AUDIT VERDICT: CLEAN (0 blocks, 0 bytes in-use)
 *   or the exact count + first offenders with their (file:line) alloc site.
 *
 * Interposition happens purely via macros, so a normal build needs no source
 * changes and the real functions are called through audit_*.c (which is
 * compiled WITHOUT the -include flag — otherwise these macros would
 * self-wrap and recurse infinitely).
 *
 * What it catches vs valgrind: unmatched (leaked) allocations per exact
 * alloc-site line, double frees, frees of unknown pointers, and peak/live
 * counts. What it does NOT catch: wild writes past the end of a block
 * (valgrind's read/write tracing) — for that the Linux `make valgrind`
 * run from docs/valgrind_baseline_HOWTO.md remains authoritative. */
#ifndef ALLOC_AUDIT_H
#define ALLOC_AUDIT_H

#include <stdlib.h>   /* real decls BEFORE the macros, so this TU's own   */
#include <stddef.h>   /* system headers are unaffected */

/* ledger entry points (also direct API for alloc_audit.c) */
void  *audit_malloc (int line, size_t size);
void  *audit_calloc (int line, size_t n, size_t size);
void  *audit_realloc(int line, void *p, size_t size);
char  *audit_strdup (int line, const char *s);
void   audit_free   (int line, void *p);
void   audit_report (void);          /* final summary (also via atexit) */

/* ---- interposition from here on ----
 * __LINE__ of the macro use site identifies the alloc site — enough to pin
 * any offender without valgrind's stack walk. */
#define malloc(a)            audit_malloc (__LINE__, (a))
#define calloc(a, b)         audit_calloc (__LINE__, (a), (b))
#define realloc(a, b)        audit_realloc(__LINE__, (a), (b))
#define strdup(s)            audit_strdup (__LINE__, (s))
#define _strdup(s)           audit_strdup (__LINE__, (s))
#define free(p)              audit_free   (__LINE__, (p))

#endif /* ALLOC_AUDIT_H */

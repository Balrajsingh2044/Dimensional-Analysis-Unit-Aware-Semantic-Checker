/* common.h — shared definitions, error reporting, and constants.
 * IMPORTANT: THIS FILE IS THE TEAM CONTRACT: DO NOT EDIT WITHOUT DISCUSSION.
 *
 * Role split:  Balraj = lexer/parser/symtab  |  Ayansh = semantic analyzer  |  Shreyansh = codegen/demo  |  Shashwat = docs/release QA
 */
#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------- SI base dimensions ----------------
 * Order is fixed across the whole project. Every UnitVec indexes into this.
 */
typedef enum {
    DIM_M   = 0, /* meter                (length)               */
    DIM_KG  = 1, /* kilogram             (mass)                 */
    DIM_S   = 2, /* second               (time)                 */
    DIM_A   = 3, /* ampere               (electric current)     */
    DIM_K   = 4, /* kelvin               (temperature)          */
    DIM_MOL = 5, /* mole                 (amount of substance)  */
    DIM_CD  = 6, /* candela              (luminous intensity)   */
    NUM_DIMENSIONS = 7
} UnitDim;

/* ---------------- diagnostics ----------------
 * Error style agreed for the whole project (Ayansh owns message wording):
 *   ERROR: Cannot add Length [1,0,0,0,0,0,0] to Time [0,0,1,0,0,0,0]
 *   (file:line) note: while checking 'distance + time'
 */
#define DIM_ERR(fmt, ...) \
    (fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__))

#define DIM_NOTE(fmt, ...) \
    (fprintf(stderr, "  note: " fmt "\n", ##__VA_ARGS__))

#define DIM_WARN(fmt, ...) \
    (fprintf(stderr, "WARNING: " fmt "\n", ##__VA_ARGS__))

/* Exit codes shared by the driver and the test runner. */
enum {
    EXIT_COMPILE_OK    = 0,  /* valid program, analysis passed */
    EXIT_COMPILE_ERROR = 1,  /* semantic/type/unit error (expected for invalid tests) */
    EXIT_USAGE         = 2,  /* bad command line */
    EXIT_BUILD_ERROR   = 3   /* lexer/parser stage failed before analysis */
};

/* MinGW-safe strdup (POSIX strdup is unavailable under -std=c11).
 * 'static' (not 'static inline'): guaranteed emitted per TU, so it links
 * at -O0 too. */
static char *dim_strdup(const char *s) __attribute__((unused));
static char *dim_strdup(const char *s) {
    size_t n;
    char *d;
    if (!s) return NULL;
    n = strlen(s) + 1;
    d = (char *)malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

#endif /* COMMON_H */

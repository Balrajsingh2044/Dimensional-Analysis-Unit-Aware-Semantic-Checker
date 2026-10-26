/* unitvec.c — 7-D unit vector engine (Ayansh).
 *
 * Everything here is plain exponent arithmetic over dim[7]:
 *   multiply -> element-wise ADD,  divide -> element-wise SUBTRACT.
 * uv_n(a,a*), uv_n(a,-a), uv_n(a,0), uv_n(a,1), uv_n(-a,0), uv_n(a,0)
 */
#include "unitvec.h"
#include <string.h>
#include <ctype.h>

static const char *g_base_names[NUM_DIMENSIONS] = {
    "m", "kg", "s", "A", "K", "mol", "cd"
};

static const char *g_dim_names[NUM_DIMENSIONS] = {
    "Length", "Mass", "Time", "Current", "Temperature", "Amount", "Intensity"
};

/* accepted spellings in the .dim language (lexer may also map these) */
typedef struct { const char *word; UnitDim dim; int exp; } UnitWord;

static const UnitWord g_words[] = {
    { "meters",      DIM_M,   1 },
    { "meter",       DIM_M,   1 },
    { "m",           DIM_M,   1 },
    { "kilometers",  DIM_M,   1 },
    { "feet",        DIM_M,   1 },
    { "foot",        DIM_M,   1 },
    { "ft",          DIM_M,   1 },
    { "kg",         DIM_KG,   1 },
    { "kilograms",   DIM_KG,  1 },
    { "grams",       DIM_KG,  1 },
    { "pounds",      DIM_KG,  1 },
    { "lb",          DIM_KG,  1 },
    { "seconds",     DIM_S,   1 },
    { "second",      DIM_S,   1 },
    { "s",           DIM_S,   1 },
    { "hours",       DIM_S,   1 },
    { "amperes",     DIM_A,   1 },
    { "ampere",      DIM_A,   1 },
    { "A",           DIM_A,   1 },
    { "kelvin",      DIM_K,   1 },
    { "K",           DIM_K,   1 },
    { "moles",       DIM_MOL, 1 },
    { "mole",        DIM_MOL, 1 },
    { "mol",         DIM_MOL, 1 },
    { "candela",     DIM_CD,  1 },
    { "cd",          DIM_CD,  1 },
    { 0, DIM_M, 0 }
};

/* ------------------------------------------------ constructors */

UnitVec uv_dimensionless(void) {
    UnitVec v;
    memset(&v, 0, sizeof v);
    return v;
}

UnitVec uv_n(int m, int kg, int s, int A, int K, int mol, int cd) {
    UnitVec v;
    v.dim[DIM_M]   = m;
    v.dim[DIM_KG]  = kg;
    v.dim[DIM_S]   = s;
    v.dim[DIM_A]   = A;
    v.dim[DIM_K]   = K;
    v.dim[DIM_MOL] = mol;
    v.dim[DIM_CD]  = cd;
    return v;
}

UnitVec uv_from_name(const char *name) {
    int i;
    if (!name) return uv_dimensionless();
    for (i = 0; g_words[i].word; i++) {
        if (strcmp(g_words[i].word, name) == 0) {
            UnitVec v = uv_dimensionless();
            v.dim[g_words[i].dim] = g_words[i].exp;
            return v;
        }
    }
    DIM_WARN("unknown unit literal '%s', treating as dimensionless", name);
    return uv_dimensionless();
}

int uv_is_unit_word(const char *word) {
    int i;
    if (!word) return 0;
    for (i = 0; g_words[i].word; i++)
        if (strcmp(g_words[i].word, word) == 0) return 1;
    return 0;
}

/* ------------------------------------------------ algebra */

UnitVec uv_mul(UnitVec a, UnitVec b) {
    UnitVec r; int i;
    for (i = 0; i < NUM_DIMENSIONS; i++)
        r.dim[i] = a.dim[i] + b.dim[i];
    return r;
}

UnitVec uv_div(UnitVec a, UnitVec b) {
    UnitVec r; int i;
    for (i = 0; i < NUM_DIMENSIONS; i++)
        r.dim[i] = a.dim[i] - b.dim[i];
    return r;
}

UnitVec uv_pow(UnitVec a, int exp) {
    UnitVec r = uv_dimensionless();
    int i;
    int neg = (exp < 0);
    if (neg) exp = -exp;
    if (neg) {
        for (i = 0; i < NUM_DIMENSIONS; i++)
            r.dim[i] = -a.dim[i] * exp;
    } else {
        for (i = 0; i < NUM_DIMENSIONS; i++)
            r.dim[i] = a.dim[i] * exp;
    }
    return r;
}

/* ------------------------------------------------ predicates */

int uv_equal(UnitVec a, UnitVec b) {
    int i;
    for (i = 0; i < NUM_DIMENSIONS; i++)
        if (a.dim[i] != b.dim[i]) return 0;
    return 1;
}

int uv_is_dimensionless(UnitVec a) {
    return uv_equal(a, uv_dimensionless());
}

int uv_is_unitary(UnitVec a, UnitDim *out_dim) {
    int i, nonzero = 0, dim = -1;
    for (i = 0; i < NUM_DIMENSIONS; i++) {
        if (a.dim[i] != 0) {
            if (a.dim[i] != 1 && a.dim[i] != -1) return 0; /* exponent != 1 */
            if (nonzero++)          return 0;             /* 2+ dims      */
            dim = i;
        }
    }
    if (out_dim && dim >= 0) *out_dim = (UnitDim)dim;
    return nonzero == 1;
}

/* ------------------------------------------------ printing */

void uv_str(UnitVec v, char *buf, size_t buflen) {
    size_t off = 0;
    int i;
    if (!buf || buflen == 0) return;
    buf[0] = '\0';
    off += (size_t)snprintf(buf + off, buflen - off, "[");
    for (i = 0; i < NUM_DIMENSIONS; i++) {
        off += (size_t)snprintf(buf + off, buflen - off,
                                i == NUM_DIMENSIONS - 1 ? "%d" : "%d,",
                                v.dim[i]);
        if (off >= buflen) break;
    }
    snprintf(buf + off, buflen > off ? buflen - off : 0, "]");
}

void uv_describe(UnitVec v, char *buf, size_t buflen) {
    int i;
    int printed = 0;
    if (!buf || buflen == 0) return;
    buf[0] = '\0';

    if (uv_is_dimensionless(v)) {
        snprintf(buf, buflen, "Scalar");
        return;
    }

    /* positive exponents (numerator):  "Length*Mass^2" */
    for (i = 0; i < NUM_DIMENSIONS; i++) {
        if (v.dim[i] > 0) {
            size_t off = strlen(buf);
            off += (size_t)snprintf(buf + off, buflen - off, printed ? "*" : "");
            if (v.dim[i] == 1)
                snprintf(buf + off, buflen - off, "%s", g_dim_names[i]);
            else
                snprintf(buf + off, buflen - off, "%s^%d", g_dim_names[i], v.dim[i]);
            printed = 1;
        }
    }
    /* negative exponents (denominator):  ".../Time^2" */
    for (i = 0; i < NUM_DIMENSIONS; i++) {
        if (v.dim[i] < 0) {
            size_t off = strlen(buf);
            off += (size_t)snprintf(buf + off, buflen - off, printed ? "/" : "1/");
            if (v.dim[i] == -1)
                snprintf(buf + off, buflen - off, "%s", g_dim_names[i]);
            else
                snprintf(buf + off, buflen - off, "%s^%d", g_dim_names[i], -v.dim[i]);
            printed = 1;
        }
    }
}

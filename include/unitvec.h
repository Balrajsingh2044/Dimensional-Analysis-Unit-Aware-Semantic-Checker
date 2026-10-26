/* unitvec.h — 7-dimensional unit vector engine (Ayansh owns this).
 * ⭐ THIS FILE IS THE TEAM CONTRACT: DO NOT EDIT WITHOUT DISCUSSION.
 *
 * A unit vector is a list of exponents over the SI base dimensions, in the order:
 *   [ m, kg, s, A, K, mol, cd ]
 *
 *   meters  => [1,0,0,0,0,0,0]
 *   seconds => [0,0,1,0,0,0,0]
 *   m/s     => [1,0,-1,0,0,0,0]
 *   m/s^2   => [1,0,-2,0,0,0,0]     (force: kg*m/s^2 => [1,1,-2,0,0,0,0])
 *   m/m     => [0,0,0,0,0,0,0]      (dimensionless / scalar)
 */
#ifndef UNITVEC_H
#define UNITVEC_H

#include "common.h"

typedef struct {
    int    dim[NUM_DIMENSIONS]; /* exponent per SI base unit; may be negative */
    double scale;               /* canonical SI factor of the word this vector
                                 * came from: meters=1, feet=0.3048, km=1000,
                                 * hours=3600, ... WEEK-3 (metric vs imperial)
                                 * — added by team agreement (design #6) */
} UnitVec;

/* ---- constructors ---- */

/* The all-zero vector: dimensionless / plain numeric constant. */
UnitVec uv_dimensionless(void);

/* Vector for a named base unit like "meters", "seconds", "kg".
 * Returns the zero vector and prints a warning on an unknown name. */
UnitVec uv_from_name(const char *name);

/* 1 if 'word' is a recognized unit literal in the .dim language.
 * Used by the lexer to decide UNIT token vs ordinary identifier.
 * (NOTE for Ayansh: one word list lives in unitvec.c — keep it the
 * single source of truth; the lexer never duplicates it.) */
int  uv_is_unit_word(const char *word);

/* Build from raw exponents: uv_n(1,0,-1,0,0,0,0) == m/s. */
UnitVec uv_n(int m, int kg, int s, int A, int K, int mol, int cd);

/* ---- vector algebra (exponent math) ---- */

/* Multiplication: exponents add element-wise. rv == product of a and b. */
UnitVec uv_mul(UnitVec a, UnitVec b);

/* Division: exponents subtract element-wise (rv == a / b).
 * uv_div(x, x) is the dimensionless vector (cancellation). */
UnitVec uv_div(UnitVec a, UnitVec b);

/* Raise to an integer power (uses uv_mul / uv_div internally). */
UnitVec uv_pow(UnitVec a, int exp);

/* ---- predicates ---- */

/* Element-wise exact equality — required for + and - (together with
 * scale_equal, so meters + feet is rejected even though both are Length). */
int  uv_equal(UnitVec a, UnitVec b);

/* Canonical-factor equality (relative tolerance) — + and - need BOTH this
 * and uv_equal on the operands. meters vs feet share the vector but not
 * the scale, so they must be rejected rather than silently converted. */
int  scale_equal(UnitVec a, UnitVec b);

/* The canonical factor stored in a vector (nn==1.0 for composite mid-air
 * vectors not built from a unit word, e.g. uv_n / uv_dimensionless). */
double uv_scale(UnitVec v);

/* 1 if every exponent is 0 (a plain scalar / constant). */
int  uv_is_dimensionless(UnitVec a);

/* 1 if the vector lies within a same 1-D scale (single nonzero exponent) —
 * used later for metric-vs-imperial checks, e.g. meters vs feet. */
int  uv_is_unitary(UnitVec a, UnitDim *out_dim);

/* ---- printing / naming ---- */

/* Human-readable string like "Length" or "Length*Mass/Time" into buf. */
void uv_describe(UnitVec v, char *buf, size_t buflen);

/* Compact numeric form "[1,0,-1,0,0,0,0]" into buf (used in error messages). */
void uv_str(UnitVec v, char *buf, size_t buflen);

#endif /* UNITVEC_H */

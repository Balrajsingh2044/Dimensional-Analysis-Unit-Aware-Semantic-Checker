/* symtab.h — hash-table symbol table (Balraj owns; read/written by Ayansh).
 * ⭐ THIS FILE IS THE TEAM CONTRACT: DO NOT EDIT WITHOUT DISCUSSION.
 *
 * Stores one entry per declared identifier:
 *   - name            e.g. "distance"
 *   - primitive type  e.g. float/double/int
 *   - UnitVec         the 7-int unit vector, e.g. m => [1,0,0,0,0,0,0]
 *
 * This table also backs the checker: Assignment inserts (let x = ...),
 * expression traversal looks up (does 'x' exist? what are its units?).
 */
#ifndef SYMTAB_H
#define SYMTAB_H

#include "common.h"
#include "unitvec.h"

/* ---------------- primitive types ---------------- */
typedef enum {
    TYPE_INT    = 0,
    TYPE_FLOAT  = 1,
    TYPE_DOUBLE = 2,
    TYPE_BOOL   = 3,
    TYPE_UNKNOWN = 4
} PrimType;

const char *prim_type_str(PrimType t);

/* ---------------- symbols & table ---------------- */

typedef struct Symbol {
    char     *name;
    PrimType  type;
    UnitVec   unit;
    double    scale;           /* canonical SI factor, default 1.0 (Week-3
                               * metric-vs-imperial: stored alongside unit) */
    int       line;            /* declaration line, for diagnostics */
    double    value;           /* Week-5 eval pass: computed numeric value
                                * in canonical SI (0.0 until interpreted) */
    struct Symbol *next;       /* collision chain (separate chaining) */
} Symbol;

typedef struct SymTab {
    Symbol **buckets;
    size_t   nbuckets;
    size_t   count;            /* live symbols; doubles trigger resize */
} SymTab;

/* ---- lifecycle ---- */
SymTab *symtab_create(void);
void    symtab_destroy(SymTab *st);

/* ---- lookup / insert ----
 * lookup returns NULL for unknown identifiers (checker turns that into
 * "undeclared variable").
 */
Symbol *symtab_lookup(SymTab *st, const char *name);

/* Inserts a fresh symbol; returns NULL if a symbol with that name already
 * exists (redeclaration error is reported by the caller). */
Symbol *symtab_insert(SymTab *st, const char *name, PrimType type,
                      UnitVec unit, int line);

/* Same, but also stores the canonical scale (Week-3 scale field, team-agreed;
 * e.g. a speed in m/h carries scale 3600). scale must be finite and > 0. */
Symbol *symtab_insert_s(SymTab *st, const char *name, PrimType type,
                        UnitVec unit, int line, double scale);

/* Re-point an existing symbol's unit (e.g. new assignment to same name). */
int     symtab_update_unit(SymTab *st, const char *name, UnitVec unit);

/* Same, but also re-points the canonical scale (analyzer's assign path). */
int     symtab_update_unit_s(SymTab *st, const char *name, UnitVec unit,
                             double scale);

/* Week-5 eval pass: store the computed canonical SI value of a declared
 * variable (the evaluator calls this on `let` / assignment). Returns 0 on
 * success, -1 if the name is unknown (should not happen: the analyzer
 * declares on every `let` before evaluation runs). */
int     symtab_update_value(SymTab *st, const char *name, double value);

/* ---- introspection (spreadsheet in the README lives on these) ---- */
void    symtab_print(SymTab *st, FILE *out);   /* debug dump */
size_t  symtab_count(SymTab *st);

#endif /* SYMTAB_H */

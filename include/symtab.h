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
    int       line;            /* declaration line, for diagnostics */
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

/* Re-point an existing symbol's unit (e.g. new assignment to same name). */
int     symtab_update_unit(SymTab *st, const char *name, UnitVec unit);

/* ---- introspection (spreadsheet in the README lives on these) ---- */
void    symtab_print(SymTab *st, FILE *out);   /* debug dump */
size_t  symtab_count(SymTab *st);

#endif /* SYMTAB_H */

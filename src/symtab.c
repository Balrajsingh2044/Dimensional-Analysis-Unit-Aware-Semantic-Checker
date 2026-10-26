/* symtab.c — hash-table symbol table (Balraj owns).
 *
 * Separate chaining; resizes at load factor ~1.
 * API contract lives in include/symtab.h.
 */
#include "symtab.h"
#include <string.h>

/* FNV-1a (32-bit constants so 32-bit size_t builds don't truncate) */
static size_t hash_str(const char *s, size_t nbuckets) {
    size_t h = 2166136261u;
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 16777619u;
    }
    return h & (nbuckets - 1);
}

static Symbol *symbol_new(const char *name, PrimType type, UnitVec unit, int line) {
    Symbol *s = (Symbol *)calloc(1, sizeof *s);
    if (!s) return NULL;
    s->name = dim_strdup(name);
    s->type = type;
    s->unit = unit;
    s->line = line;
    return s;
}

static void resize(SymTab *st) {
    size_t old_n = st->nbuckets, i;
    Symbol **old = st->buckets;
    st->nbuckets = old_n * 2;
    st->buckets = (Symbol **)calloc(st->nbuckets, sizeof *st->buckets);
    for (i = 0; i < old_n; i++) {
        Symbol *s = old[i], *nx;
        while (s) {
            nx = s->next;
            size_t idx = hash_str(s->name, st->nbuckets);
            s->next = st->buckets[idx];
            st->buckets[idx] = s;
            s = nx;
        }
    }
    free(old);
}

/* ------------------------------------------------ lifecycle */

SymTab *symtab_create(void) {
    SymTab *st = (SymTab *)calloc(1, sizeof *st);
    if (!st) return NULL;
    st->nbuckets = 64;
    st->buckets = (Symbol **)calloc(st->nbuckets, sizeof *st->buckets);
    if (!st->buckets) { free(st); return NULL; }
    return st;
}

void symtab_destroy(SymTab *st) {
    size_t i;
    if (!st) return;
    for (i = 0; i < st->nbuckets; i++) {
        Symbol *s = st->buckets[i];
        while (s) {
            Symbol *nx = s->next;
            free(s->name);
            free(s);
            s = nx;
        }
    }
    free(st->buckets);
    free(st);
}

/* ------------------------------------------------ lookup / insert */

Symbol *symtab_lookup(SymTab *st, const char *name) {
    Symbol *s;
    if (!st || !name) return NULL;
    s = st->buckets[hash_str(name, st->nbuckets)];
    for (; s; s = s->next)
        if (strcmp(s->name, name) == 0) return s;
    return NULL;
}

Symbol *symtab_insert(SymTab *st, const char *name, PrimType type,
                      UnitVec unit, int line) {
    size_t idx;
    Symbol *s;
    if (!st || !name) return NULL;
    if (symtab_lookup(st, name)) return NULL;    /* redeclaration */
    if ((st->count + 1) > st->nbuckets) resize(st);

    s = symbol_new(name, type, unit, line);
    if (!s) return NULL;
    idx = hash_str(name, st->nbuckets);
    s->next = st->buckets[idx];
    st->buckets[idx] = s;
    st->count++;
    return s;
}

int symtab_update_unit(SymTab *st, const char *name, UnitVec unit) {
    Symbol *s = symtab_lookup(st, name);
    if (!s) return -1;
    s->unit = unit;
    return 0;
}

/* ------------------------------------------------ introspection */

const char *prim_type_str(PrimType t) {
    switch (t) {
        case TYPE_INT:    return "int";
        case TYPE_FLOAT:  return "float";
        case TYPE_DOUBLE: return "double";
        case TYPE_BOOL:   return "bool";
        default:          return "unknown";
    }
}

size_t symtab_count(SymTab *st) { return st ? st->count : 0; }

void symtab_print(SymTab *st, FILE *out) {
    size_t i;
    char ubuf[64];
    fprintf(out, "── symbol table (%zu symbols) ──\n", symtab_count(st));
    for (i = 0; i < st->nbuckets; i++) {
        Symbol *s;
        for (s = st->buckets[i]; s; s = s->next) {
            uv_str(s->unit, ubuf, sizeof ubuf);
            fprintf(out, "  %-12s %-8s %s  (decl line %d)\n",
                    s->name, prim_type_str(s->type), ubuf, s->line);
        }
    }
}

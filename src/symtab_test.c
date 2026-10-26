/* symtab_test.c — starter contract test for the symbol table (Balraj).
 * Run: make symtab_test && ./symtab_test
 * Verifies the API Ayansh's analyzer will rely on before the parser exists.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "symtab.h"

static int g_pass = 0, g_fail = 0;
static void check_impl(int cond, const char *what) {
    if (cond) { g_pass++; printf("  ok  %s\n", what); }
    else      { g_fail++; printf("FAIL  %s\n", what); }
}
#define CHECK(cond, what) check_impl((cond), (what))

int main(void) {
    SymTab *st = symtab_create();
    Symbol *s;

    puts("[1] insert + lookup");
    s = symtab_insert(st, "distance", TYPE_DOUBLE, uv_from_name("meters"), 1);
    CHECK(s != NULL, "insert 'distance' as double [1,0,0,0,0,0,0]");
    CHECK(symtab_lookup(st, "distance") == s, "lookup returns same symbol");
    CHECK(s->unit.dim[DIM_M] == 1, "stored unit has m^1");

    puts("[2] redeclaration rejected");
    CHECK(symtab_insert(st, "distance", TYPE_DOUBLE, uv_from_name("seconds"), 2) == NULL,
          "re-insert 'distance' returns NULL");

    puts("[3] unknown identifier");
    CHECK(symtab_lookup(st, "spam") == NULL, "lookup 'spam' is NULL (undeclared var)");

    puts("[4] unit update (reassignment path)");
    CHECK(symtab_update_unit(st, "distance", uv_mul(uv_from_name("meters"),
                                                    uv_from_name("meters"))) == 0,
          "re-assign distance => m^2");
    CHECK(symtab_lookup(st, "distance")->unit.dim[DIM_M] == 2, "distance now m^2");
    CHECK(symtab_update_unit(st, "spam", uv_from_name("seconds")) != 0,
          "update on unknown symbol fails cleanly");

    puts("[5] many symbols (hash resize path)");
    char name[32];
    int i, ok = 1;
    for (i = 0; i < 500; i++) {
        snprintf(name, sizeof name, "var_%d", i);
        if (!symtab_insert(st, name, TYPE_FLOAT, uv_dimensionless(), i + 10)) ok = 0;
    }
    CHECK(ok, "insert 500 symbols");
    snprintf(name, sizeof name, "var_%d", 417);
    CHECK(symtab_lookup(st, name) != NULL, "lookup after resize works");
    CHECK(symtab_count(st) == 501, "count == 501");

    puts("[6] canonical-scale API (Week-3 metric vs imperial)");
    s = symtab_insert_s(st, "speed_h", TYPE_DOUBLE, uv_from_name("meters"),
                        11, 3600.0);        /* meters/hour */
    CHECK(s != NULL && s->scale == 3600.0, "insert_s stores the scale");
    CHECK(symtab_insert(st, "speed_kmh", TYPE_DOUBLE, uv_from_name("seconds"), 12) != NULL &&
          symtab_lookup(st, "speed_kmh")->scale == 1.0,
          "plain insert defaults scale to 1.0");
    CHECK(symtab_update_unit_s(st, "speed_h", uv_from_name("feet"), 0.3048) == 0 &&
          fabs(symtab_lookup(st, "speed_h")->scale - 0.3048) < 1e-9,
          "update_unit_s re-points the scale");
    CHECK(symtab_update_unit_s(st, "spam", uv_from_name("feet"), 0.3048) != 0,
          "update_unit_s on unknown symbol fails cleanly");

    symtab_print(st == NULL ? NULL : st, stdout);  /* just proves it doesn't crash */
    symtab_destroy(st);
    CHECK(1, "destroy (clean under Valgrind: valgrind ./symtab_test)");

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}

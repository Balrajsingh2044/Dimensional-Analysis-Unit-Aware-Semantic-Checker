# Valgrind baseline — HOWTO and report template (Ayansh, final QA deliverable)

## How to run (any Linux or WSL with valgrind)

```bash
# 0) get a clean checkout (or reuse an existing one)
git pull origin main

# 1) build everything
make

# 2) run the baseline (three contexts: engine, table, full checker)
make valgrind 2>&1 | tee valgrind_baseline_raw.txt

# (recommended extra) the invalid path too — error paths allocate/free differently
valgrind --leak-check=full --error-exitcode=9 ./output/dimc-checker tests/invalid/01_dist_plus_time.dim
valgrind --leak-check=full --error-exitcode=9 ./output/dimc-checker tests/invalid/04_metric_imperial.dim
```

Note: if your valgrind is installed but `make valgrind` misses it, run the
three commands inside the Makefile's `valgrind:` target by hand.

## Report format — fill below and rename to valgrind_report.md (do NOT commit raw dumps)

```
# Valgrind Baseline Report (Ayansh)

Build:  commit <SHA>; gcc <version>; valgrind <version>; Linux/WSL <distro + release>
Date:   <date>

PASS criteria: each context = 0 errors / 0 leak blocks (All heap blocks
freed; in-use at exit: 0 bytes in 0 blocks).

| context                      | errors | in-use at exit | verdict |
|------------------------------|--------|----------------|---------|
| ./output/vec_test            | ___    | ___            | ___     |
| ./output/symtab_test         | ___    | ___            | ___     |
| ./output/dimc-checker tests/valid/00_basic_fe_check.dim  | ___ | ___ | ___ |
| ./output/dimc-checker tests/invalid/01_dist_plus_time.dim (error path) | ___ | ___ | ___ |
| ./output/dimc-checker tests/invalid/04_metric_imperial.dim (scale path) | ___ | ___ | ___ |

Total: PASS / FAIL (circle)

Notes / anomalies found:
- <e.g. "all 5 contexts clean">
```

## Expected: LEAK CHECK should be clean everywhere

 constructors/destructors pairs to watch when reading any burst output:

- `symtab_create/destroy`, `symtab_insert` strdup'd names freed in destroy (Balraj)
- `ast_new_*` / `ast_free`: name + unit_name owned by node (both members)
- word tables are `static const` — never freed, never leaked (static storage)

If a burst mentions a line inside `semantic.c` folding — capture the exact
stack (paste into this file) and tell the group BEFORE changing anything:
ownership decisions are contract-level.

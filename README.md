# Dimensional Analysis — Unit-Aware Semantic Checker

A compiler front end for a small physics language (`.dim` files) that treats
physical units as **first-class citizens**: `meters + seconds` is a
**compile-time error**, `kg * meters / seconds^2` (force) checks out, and the
output is unit-stripped plain C — **zero runtime overhead**.

## Pipeline

```
.dim source ──(lexer.l)──▶ tokens ──(parser.y)──▶ AST ──(semantic.c)──▶ checked AST ──(codegen.c)──▶ C
                    Balraj                    Ayansh                     Shreyansh
```

## Team

| Member  | Owns  |
|---------|------|
| Balraj  | Lexer (Flex), Parser (Bison), `ast.c`, Symbol Table, tooling |
| Ayansh  | Semantic analyzer: 7-D unit vector engine, AST folding, diagnostics |
| Shreyansh | Codegen, test suite, demo/presentation |

## Repository layout

```
├── include/          ⭐ shared contract: common.h, unitvec.h, symtab.h, ast.h
├── src/
│   ├── lexer.l, parser.y, ast.c, symtab.c, frontend_main.c   → Balraj
│   ├── unitvec.c, semantic.c, semantic_ops.c                  → Ayansh
│   └── (codegen.c, main.c later)                              → Shreyansh
│   └── vec_test.c, symtab_test.c, semantic_test.c             → contract tests
├── tests/            valid/ + invalid/ .dim programs + run script (Shreyansh)
├── demo/             demo programs (Shreyansh)
├── docs/             slides, diagrams, design decisions
└── output/           build artifacts (gitignored)
```

**Rule:** `include/*.h` is the only contract between members — change it only
by agreement. Everything else merges on integration points.

## Build (Linux / WSL — use the Makefile)

```sh
make            # contract tests + dimc-checker (+ dimc automatically once
                # src/main.c AND src/codegen.c exist in the repo)
make test       # runs the contract tests
make checker FILE=tests/valid/00_basic_fe_check.dim   # front end + analyzer
```

**Two binaries — don't mix them up:**

| binary | contains | owner |
|---|---|---|
| `output/dimc-checker` | lexer/parser/AST + symbol table + **semantic analyzer** (debug dumps) | Balraj |
| `output/dimc` | all of the above + **codegen** (final deliverable) | Shreyansh (`src/main.c`) |

## Test-runner contract (for run_tests.sh — grep these, do NOT parse the AST dump)

Frozen output of `output/dimc-checker FILE.dim`:

| case | stdout contract | stderr | exit |
|---|---|---|---|
| valid program | `front end ok: <N> symbol(s)` **and** `semantic ok: <N> symbol(s), 0 error(s)` (each exactly once) + debug AST/symtab block | possibly warnings | 0 |
| parse error | NO contract lines | `ERROR: parse error (line N): …` | 3 (`EXIT_BUILD_ERROR`) |
| semantic error | `front end ok: …` printed, **no** `semantic ok:` line | `ERROR: semantic check failed…` + filtered per-error diagnostics | 1 (`EXIT_COMPILE_ERROR`) |
| bad usage / no file | — | usage / `cannot open` | 2 (`EXIT_USAGE`) |

Suggested greps (safe against the debug dumps):
```sh
grep -Fq 'front end ok:' out.txt && \
grep -Fq 'semantic ok'   out.txt   # valid case
# invalid case uses the exit code + grep -F of the expected diagnostic text
```


**Semantic status (Integration Point #2 DONE).** `dimc` now runs the full
unit-aware checker: `declared: speed : Length/Time [1,0,-1,0,0,0,0]` lines,
and exact diagnostics like

```
ERROR: Cannot add Length [1,0,0,0,0,0,0] to Time [0,0,1,0,0,0,0]
  note: line 4: while checking '(distance + time)'
```

## Build on Windows (no make installed — direct commands, from repo root)

```bat
mkdir output
bison -d -o output\parser.tab.c src\parser.y
cmd /c "flex -t src\lexer.l > output\lexer.yy.c"
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -Ioutput -c output\lexer.yy.c -o output\lexer.yy.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -Ioutput -c output\parser.tab.c -o output\parser.tab.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -Ioutput -c src\ast.c -o output\ast.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -Ioutput -c src\unitvec.c -o output\unitvec.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -Ioutput -c src\symtab.c -o output\symtab.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -c src\semantic.c -o output\semantic.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -c src\semantic_ops.c -o output\semantic_ops.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -Ioutput -c src\frontend_main.c -o output\frontend_main.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -Ioutput -o output\dimc-checker.exe output\lexer.yy.o output\parser.tab.o output\ast.o output\symtab.o output\unitvec.o output\semantic.o output\semantic_ops.o output\frontend_main.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -o output\vec_test.exe src\vec_test.c output\unitvec.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -o output\symtab_test.exe src\symtab_test.c output\symtab.o output\unitvec.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -o output\semantic_test.exe src\semantic_test.c output\semantic.o output\semantic_ops.o output\symtab.o output\unitvec.o output\ast.o

output\vec_test.exe          REM 20 checks
output\symtab_test.exe       REM 12 checks
output\semantic_test.exe     REM 15 checks (semantic analyzer)
output\dimc-checker.exe YOUR_FILE.dim
REM (`output\dimc.exe` appears here only after src/main.c + src/codegen.c land)
```

## The .dim language (as of Week 1)

```
let distance = 5 * meters;
let time     = 10 * seconds;
let speed    = distance / time;
let force    = 2 * kg * meters / (seconds ^ 2);
```

- `let` declares, plain `name = ...` assigns (must already exist).
- Unit literals (`meters`, `kg`, `seconds`, `K`, `A`, `cd`, `mol`, plus
  `feet`, `hours`, ...) are **first-class tokens**.
- Single-letter aliases `m`, `s`, `kg`, ... are reserved as units — don't use
  them as variable names.

## Known notes for teammates

- **Ayansh:** Phase 3 is DONE and integrated (see `include/semantic.h`).
  `semantic_check(st, prog)` is the only entry point the driver needs.
  Note for the diagnostics you'll see: the operator table prints are
  anchored at `OP_ADD=1` (an earlier off-by-one was fixed in
  `semantic_ops.c`). `g_base_names[]` is still unused — fine.
- **Ayansh -> Balraj:** `frontend_main.c` now calls `semantic_check()` and
  `collect_decls()` is retired (`#if 0` block explains why): the analyzer
  declares symbols itself in source order, which makes use-before-declare
  detectable. If you want the old front-end-only dump back, stop at the
  `front end ok:` line or flag it out later.
- **Shreyansh (codegen handoff):** every expression node now has
  `unit_known==1` and a folded `node->unit` after analysis; use
  `node->prim` (TYPE_DOUBLE / TYPE_INT from `sem_node_type()`) to pick a
  C type: units or fractions -> `double`, dimensionless ints -> `int`.
  Print greppable line `front end ok: N symbol(s)` stays in the driver.
  Keep `semantic ok:` line too — the test runner greps both.
- **Shreyansh:** new `semantic_test.exe` (15 checks) is part of
  `make test`; add it to `tests/run_tests.sh`.
- **Shreyansh (metric-vs-imperial, Phase 3 leftover):** `meters` and
  `feet` both fold to Length, so `5 * meters + 3 * feet` currently PASSES.
  Catching that needs a conversion-scale field (e.g. `double scale;` on
  AST units + symtab) — that is a team-contract change to `include/ast.h`/
  `include/symtab.h`; we agreed to schedule it as Week 3 work. Flags are
  already set: `uv_is_unitary()` marks base-scale-compatible vectors.
- **All:** parse errors exit with code 3 (`EXIT_BUILD_ERROR`), semantic
  errors use 1 (`EXIT_COMPILE_ERROR`) per `include/common.h`. Semantic
  errors do NOT stop the pass — one run reports every violation in a file.
- This Windows box has an old bison (2.4.1): **do not** re-add a
  `%destructor` on `<node>` — it corrupts values already consumed by actions.
  Token (`ID`/`UNIT`) destructors are fine.

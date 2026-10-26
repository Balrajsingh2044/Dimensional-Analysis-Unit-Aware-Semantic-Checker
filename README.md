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
| `output/dimc` | all of the above + **codegen** — the final deliverable | Shreyansh (`src/main.c`, `src/codegen.c`) |

Note: both drivers share the skeleton in `src/frontend_main.c` — the checker
build compiles that file **twice**: plain (`frontend_main.o`, `main()`
compiled out via `BUILD_CHECKER`) and with `-DBUILD_CHECKER`
(`frontend_checker.o`, keeps the debug `main()`). The unified `dimc` links
`frontend_main.o` + `codegen.o` + `main.o`; see the Makefile.

## The unified `./dimc` (Week 4 — LIVE)

```
dimc FILE.dim [out.c]        # out.c optional; default output on stdout
```

Pipeline: lexer → bison parse → semantic_check (fresh SymTab) →
codegen_emit. All diagnostics (parse errors, `ERROR:` unit violations,
per-scope `declared:` lines) go to **stderr** — **stdout is pure
compilable C**, so `./dimc x.dim | gcc -std=c11 -x c -` just works and the
output diffs cleanly.

Generated-C style (for demo talking points):
- every unit was checked at compile time and **stripped**: zero runtime
  unit overhead, no unit tokens survive into the C;
- constant subtrees fold to one number (`5 * meters` → `5`,
  `meters / meters` → `1`, full-chain `^` too);
- int declarations only for dimensionless integer constants, else `double`;
- `b ^ e` expands to plain multiplication for `|e| <= 8`, `pow()` beyond;
- minimal parentheses via precedence classes (left-assoc `+ - * /`,
  `^` binds tighter).

Exit codes (same codes as the checker): 0 ok · 1 semantic error ·
3 parse error · 2 usage / I-O error. On 1 or 3, **nothing** is emitted on
stdout.

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
unit-aware checker, and exact diagnostics like

```
ERROR: Cannot add Length [1,0,0,0,0,0,0] to Time [0,0,1,0,0,0,0]
  note: line 4: while checking '(distance + time)'
```

**Stream discipline (Week-4 integration fix):** the per-`let`
`declared: speed : Length/Time [1,0,-1,0,0,0,0]` lines now go to
**stderr** (`semantic.c`) — they were printing to stdout and polluting the
codegen C output (non-C text before the `/* generated by dimc */` header
breaks `gcc -x c -`, and is why "size_t" cascades appeared in early battery
runs). Rule going forward: **stdout carries only the contract lines and the
generated C; everything debug goes to stderr.**

## Build on Windows (no make installed — direct commands, from repo root)

```bat
REM if bison ever complains about "missing c-skel"/m4 data (Ayansh's tip),
REM first:  set BISON_PKGDATADIR=C:\PROGRA~2\GnuWin32\share\bison
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
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -Ioutput -c src\semantic.c -o output\semantic.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -c src\semantic_ops.c -o output\semantic_ops.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -Ioutput -c src\frontend_main.c -o output\frontend_main.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -Ioutput -DBUILD_CHECKER -c src\frontend_main.c -o output\frontend_checker.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -Ioutput -c src\codegen.c -o output\codegen.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -Ioutput -c src\main.c -o output\main.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -Ioutput -o output\dimc-checker.exe output\lexer.yy.o output\parser.tab.o output\ast.o output\symtab.o output\unitvec.o output\semantic.o output\semantic_ops.o output\frontend_checker.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -Ioutput -o output\dimc.exe output\lexer.yy.o output\parser.tab.o output\ast.o output\symtab.o output\unitvec.o output\semantic.o output\semantic_ops.o output\frontend_main.o output\codegen.o output\main.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -o output\vec_test.exe src\vec_test.c output\unitvec.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -o output\symtab_test.exe src\symtab_test.c output\symtab.o output\unitvec.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -o output\semantic_test.exe src\semantic_test.c output\semantic.o output\semantic_ops.o output\symtab.o output\unitvec.o output\ast.o

output\vec_test.exe          REM 33 checks
output\symtab_test.exe       REM 16 checks
output\semantic_test.exe     REM 22 checks (semantic analyzer)
output\dimc-checker.exe YOUR_FILE.dim
output\dimc.exe YOUR_FILE.dim [out.c]
```

## The .dim language (as of Week 2)

```
let distance = 5 * meters;
let time     = 10 * seconds;
let speed    = distance / time;
let force    = 2 * kg * meters / (seconds ^ 2);
let energy   = speed * speed;          // works: mul of same units
let tiny     = 1.5e-3 * seconds;       // scientific notation
let neg      = -3 * meters;            // unary minus (desugared to -1 * x)
```

- `let` declares, plain `name = ...` assigns (must already exist).
- Unit literals (`meters`, `kg`, `seconds`, `K`, `A`, `cd`, `mol`, plus
  `feet`, `hours`, ...) are **first-class tokens**.
- **Canonical scale (Week-3):** every unit word carries its SI factor from
  `unitvec.c`'s word table — SI atoms ×1; `feet/foot/ft` ×0.3048,
  `inches` ×0.0254, `kilometers/km` ×1000, `grams` ×0.001,
  `pounds/lb` ×0.45359237, `minutes` ×60, `hours` ×3600. `*`/`/`/`^`
  propagate scales (multiply / divide / raise). `+`/`-` require BOTH
  identical dimensions AND equal scale — so `5 * meters + 3 * feet` is an
  ERROR even though both are Length.
- Single-letter aliases `m`, `s`, `kg`, ... are reserved as units — don't use
  them as variable names.
- Numbers: integer, decimal, and scientific notation (`1.5e-3`).
- `^` chains are **left-associative and folded**: `meters ^ 3 ^ 2` produces
  one `POW(meters, 6)` AST node with exponent `3*2` ((b^a)^c == b^(a*c)).
  Exponents must be plain numbers (per semantic rules, fractional exp = ERROR).
- Unary minus binds looser than `^`: `-x ^ 2` == `-(x ^ 2)` (C-style).
- Parse errors → exit 3; semantic errors → exit 1 (see contract table above).

## Known notes for teammates

- **Ayansh (Valgrind baseline, Week-2 ask):** the `make valgrind` baseline is
  wired (vec_test + symtab_test + dimc-checker on `00_basic_fe_check.dim`).
  Balraj's Windows box has NO valgrind and NO WSL — **please run it once on
  your machine or Shreyansh's** and paste the leak summary here; there are
  known-clean paths in principle (`symtab_destroy`, `ast_free` on success
  AND error paths, token destructors) but an actual valgrind run is the
  deliverable. If you find leaks, first suspects: parser error-path nodes
  (bison 2.4.1 has no usable `<node>` destructor — see note at bottom).
- **Ayansh (collect_decls retirement — no Week-3 conflict):** audit DONE,
  agreed on both counts. The Week-3 symbol-table performance work (resize
  benchmarks, deeper hashing) is orthogonal to who inserts symbols — the
  analyzer inserting in source order makes no difference to symtab.c.

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
- **Shreyansh (codegen handoff — UPDATED for Week 4):** the unified
  pipeline is now LIVE: `src/main.c` (quiet driver: parse → semantic →
  codegen, exits 3/1/0, nothing but C on stdout) and `src/codegen.c`
  (unit-stripped emission, constant folding incl. unit literals as
  magnitude 1, minimal parens, inline `^` expansion for |e| ≤ 8, seen-list
  for LET vs re-ASSIGN) were fleshed out by Balraj during the Week-4
  integration pass **under the agreed API** — `codegen_emit(prog, outname)`
  untouched, outname==NULL → stdout. Tune/replace internals freely, but
  keep the stream discipline: **pure C on stdout, diagnostics on stderr**,
  and read the units from the AST (`node->unit`, `node->prim` — codegen
  does not touch the SymTab).
- **Shreyansh:** new `semantic_test.exe` (22 checks) is part of
  `make test`; add it to `tests/run_tests.sh`.
- **All (metric-vs-imperial — RESOLVED, Week-3 scale field landed):**
  `double scale` now lives on `UnitVec` (unitvec.h), `AstNode`
  (include/ast.h) and `Symbol` (include/symtab.h); unit words carry their
  canonical SI factors from `unitvec.c`'s single word table (meters=1,
  km=1000, feet=0.3048, inches=0.0254, grams=0.001, pounds=0.45359237,
  minutes=60, hours=3600; SI atoms = 1). `+`/`-` now require
  `uv_equal() && scale_equal()`, so `5 * meters + 3 * feet` is a HARD
  ERROR ("same dimension but different unit size — metric vs imperial?"),
  `tests/invalid/04_metric_imperial.dim` is a caught error case (no skip),
  and scale algebra propagates exactly like exponents: mul→multiply,
  div→divide, pow→raise (so `5 * kilometers / meters` even folds to the
  correct SI value 5000 in generated C). Contract-side API additions:
  `scale_equal()`, `uv_scale()`, `symtab_insert_s()`,
  `symtab_update_unit_s()`; legacy `symtab_insert()`/`symtab_update_unit()`
  unchanged for all existing callers.
- **All:** parse errors exit with code 3 (`EXIT_BUILD_ERROR`), semantic
  errors use 1 (`EXIT_COMPILE_ERROR`) per `include/common.h`. Semantic
  errors do NOT stop the pass — one run reports every violation in a file.
- **All (Week-4):** `frontend_main.c` is guarded by `BUILD_CHECKER`; the
  plain `frontend_main.o` build has that `main()` compiled out, so the
  unified `dimc` links `frontend_main.o` + `codegen.o` + `src/main.c`'s
  `main()` without a multiple-definition clash — the checker binary uses
  `frontend_checker.o` (compiled with `-DBUILD_CHECKER`), which keeps the
  debug `main()`.
- This Windows box has an old bison (2.4.1): **do not** re-add a
  `%destructor` on `<node>` — it corrupts values already consumed by actions.
  Token (`ID`/`UNIT`) destructors are fine.

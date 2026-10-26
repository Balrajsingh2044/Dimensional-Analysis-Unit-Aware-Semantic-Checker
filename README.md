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

| Member  | Owns |
|---------|------|
| Balraj  | Lexer (Flex), Parser (Bison), `ast.c`, Symbol Table, tooling |
| Ayansh  | Semantic analyzer: 7-D unit vector engine, AST folding, diagnostics |
| Shreyansh | Codegen, test suite, demo/presentation |

## Repository layout

```
├── include/          ⭐ shared contract: common.h, unitvec.h, symtab.h, ast.h
├── src/
│   ├── lexer.l, parser.y, ast.c, symtab.c, frontend_main.c   → Balraj
│   ├── unitvec.c, (semantic.c later)                          → Ayansh
│   └── (codegen.c, main.c later)                              → Shreyansh
│   └── vec_test.c, symtab_test.c                              → contract tests
├── tests/            valid/ + invalid/ .dim programs + run script (Shreyansh)
├── demo/             demo programs (Shreyansh)
├── docs/             slides, diagrams, design decisions
└── output/           build artifacts (gitignored)
```

**Rule:** `include/*.h` is the only contract between members — change it only
by agreement. Everything else merges on integration points.

## Build (Linux / WSL — use the Makefile)

```sh
make            # builds vec_test, symtab_test, dimc
make test       # runs the contract tests
make fe FILE=demo/basic_fe_check.dim   # front end only: parse + AST + symtab
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
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -Ioutput -c src\frontend_main.c -o output\frontend_main.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -Ioutput -o output\dimc.exe output\lexer.yy.o output\parser.tab.o output\ast.o output\symtab.o output\unitvec.o output\frontend_main.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -o output\vec_test.exe src\vec_test.c output\unitvec.o
gcc -Wall -Wextra -g -O0 -std=c11 -Iinclude -o output\symtab_test.exe src\symtab_test.c output\symtab.o output\unitvec.o

output\vec_test.exe          REM 20 checks
output\symtab_test.exe       REM 12 checks
output\dimc.exe YOUR_FILE.dim
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

- **Ayansh:** `unitvec.c` now exposes `uv_is_unit_word()` (used by the lexer)
  — the unit word list in `unitvec.c` is the single source of truth.
  `g_base_names[]` is currently unused; fold it into your diagnostics if you
  want base-unit names in error text.
- **Shreyansh:** `frontend_main.c` (Balraj's front-end driver) prints
  `front end ok: N symbol(s)` — the test runner can grep that. Real main.c /
  codegen replaces/wraps it later.
- **All:** parse errors exit with code 3 (`EXIT_BUILD_ERROR`), semantic
  errors will use 1 (`EXIT_COMPILE_ERROR`) per `include/common.h`.
- This Windows box has an old bison (2.4.1): **do not** re-add a
  `%destructor` on `<node>` — it corrupts values already consumed by actions.
  Token (`ID`/`UNIT`) destructors are fine.

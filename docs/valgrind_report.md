# Allocation & Leak Baseline Report (answer to Ayansh's valgrind ask)

**Title per template:** Valgrind Baseline Report — filled by Balraj
**Prepared by:** Balraj
**Build:** commit `9c924d3` + qa commit (this one); gcc 6.3.0 (MinGW 32-bit);
flex 2.5.4; bison 2.4.1; Windows 10, **NO valgrind and NO WSL installed**
(verified Oct 27, 2026 — Ayansh assumes a Linux/WSL box in the message; this
machine does not have one, see "Method").
**Date:** 2026-10-27

## Method — why not literally valgrind

`valgrind`, WSL and Dr Memory are all absent here (checked Oct 27). Since
Ayansh's deliverable is *the leak baseline*, not the tool itself, the runs
below use a portable **allocation ledger** (`src/alloc_audit.h/.c`, new in
this commit):

- compiled in via `-include src/alloc_audit.h` — NO contract header, source,
  driver or `include/` file changes in normal builds;
- interposes `malloc/calloc/realloc/strdup/_strdup/free` in ALL our objects
  **and** the generated flex/bison code (scanner buffers, parser strdups);
- keeps an address-keyed ledger → reports **in-use at exit** (same metric as
  valgrind's leak summary), plus double/foreign frees and peak live count;
- prints via `atexit`, so "0 errors / 0 bytes in-use at exit" reads exactly
  like valgrind's verdict line.

A Linux cross-check is one command: `make audit` (targets added to the
Makefile) — the ledger is cross-platform by construction, and the canonical
valgrind run from `docs/valgrind_baseline_HOWTO.md` stays welcome on
Shreyansh's machine as the write-tracing authority.

## PASS criteria (from the HOWTO)

Each context: `0 errors / 0 leak blocks / in-use at exit: 0 bytes in 0 blocks`.

## Results (Audit Run, Oct 27 2026, allocation ledger)

| context                       | exit | errors (bad/foreign frees) | in-use at exit        | verdict |
|-------------------------------|------|----------------------------|-----------------------|---------|
| ./output/audit/vec_test       | 0    | 0                          | 0 blocks / 0 bytes¹   | PASS    |
| ./output/audit/symtab_test    | 0    | 0                          | 0 blocks / 0 bytes²   | PASS    |
| ./output/audit/semantic_test  | 0    | 0                          | 0 blocks / 0 bytes    | PASS³   |
| ./output/audit/dimc-checker tests/valid/00_basic_fe_check.dim | 0 | 0 | 0 blocks / 0 bytes | PASS |
| ./output/audit/dimc-checker tests/invalid/01_dist_plus_time.dim (semantic error path) | 1 | 0 | 0 blocks / 0 bytes | PASS |
| ./output/audit/dimc-checker tests/invalid/04_metric_imperial.dim (scale error path) | 1 | 0 | 0 blocks / 0 bytes | PASS |
| ./output/audit/dimc-checker tests/invalid/05_missing_semicolon.dim (parse error path) | 3 | 0 | **4 blocks / 343 bytes** | FAIL → see finding B |
| ./output/audit/dimc.exe demo/mars_valid.dim (unified pipeline happy path) | 0 | 0 | 0 blocks / 0 bytes | PASS |
| ./output/audit/dimc.exe tests/invalid/04_metric_imperial.dim (pipeline error path) | 1 | 0 | 0 blocks / 0 bytes | PASS |

¹ vec_test performs **zero heap allocations by design** (pure stack vectors);
the run still prints the ledger line (`total=0 alloc(s)`) proving it.
² symtab_test: 1011 allocations total (incl. the 500-symbol resize stress),
all matched.
³ THIS TABLE INCLUDES A FIX, see finding A.

Sample clean verdict line (ctx 5, semantic-error path):

```
ALLOC-AUDIT: total=26 alloc(s), peak live=9, in-use at exit: 0 block(s), 0 byte(s), 0 bad free(s)
ALLOC-AUDIT VERDICT: CLEAN
```

## Finding A (fixed): missing `ast_free` in `src/semantic_test.c` §1

The audit flagged **11 blocks / 917 bytes** with all alloc sites at
`ast.c:11` (`ast_new`'s `calloc`) + `common.h:59` (`dim_strdup`) — exactly
the section-1 velocity program (8 AST nodes + 3 identifier/unit strdups).
The test file declared `speed = distance / time` and never freed it before
reassigning `prog`. **Owner of the leaked memory is the test file, not the
analyzer** — per Ayansh's rule ("don't fix unilaterally IF the leak touches
semantic.c/unitvec.c") this was a Balraj-domain correction: one
`ast_free(prog)` added. After the fix: **semantic_test = 162 allocs, 0
in-use, CLEAN**.

## Finding B (captured — NOT fixed): in-flight AST nodes on the parse-error path

`05_missing_semicolon.dim` (exit 3) leaves in-use at exit:

```
ALLOC-AUDIT: total=7 alloc(s), peak live=7, in-use at exit: 4 block(s), 343 byte(s), 0 bad free(s)
ALLOC-AUDIT LEAK: 112 byte(s) allocated by src line 11  (ast.c:11 — ast_new node)   ×3
ALLOC-AUDIT LEAK:   7 byte(s) allocated by src line 59  (common.h:59 — dim_strdup token string)
ALLOC-AUDIT VERDICT: LEAK (4 block(s), 343 byte(s), 0 bad free(s))
```

Attribution: the parser builds `LET x = 5 * meters` far enough to allocate
3 nodes + the token string, but the `;` is missing, so bison aborts before
any rule that had attached those nodes to `g_program` — the drivers only
free what the statement list holds.

**Exactly as Ayansh predicted** in the HOWTO ("first suspects: parser
error-path nodes — bison 2.4.1 has no usable `%destructor` for `<node>`").
The clean fix (`%destructor`) is the KNOWN-BROKEN option on this bison
version (it corrupts values already consumed by actions). Other fixes
(canvas of owned nodes, manual release cascade in `yyerror`) are parser
structural changes — **ownership decisions there are contract-level**, so
per Ayansh's rule the leak is only documented, with decision requested:
(a) accept as bounded known residue (4 blocks/343 bytes ONLY on files with
parse errors; never on valid programs or semantic-error programs), or
(b) schedule a parser-side release-tree design in a dedicated session.

## Finding C (fixed): flex 2.5.4's process-lifetime scanner buffer (before)

Every full-pipeline context originally showed **2 blocks / 16,426 bytes**
([40-byte YY_BUFFER_STATE + 16,386-byte yy_ch_buf] allocated by the
generated scanner, kept for process lifetime by design of flex 2.5.4).
This is Balraj's own stage, so it was fixed rather than forgiven: `lexer.l`
now exposes `lexer_release_scanner()` (a `yy_delete_buffer` wrapper) and
both drivers call it on every exit path. Observable behavior unchanged:
contract lines/exit codes/GC-output are identical (regression suite
re-green after the change: vec 33, symtab 16, semantic 22, runner 16/16).

## Pairs verified clean (per the HOWTO checklist)

- `symtab_create`/`symtab_destroy`: buckets + all 1000+ symbol/names freed.
- `ast_new_*`/`ast_free`: name and unit_name both owned and freed (scale
  field Week-3 did not introduce any allocation).
- static word tables: never allocated, never leaked, as expected.
- shreyansh's codegen seen-list: freed on every path.

## Cross-check for the team (Linux/WSL, optional but nice)

```bash
git pull
make clean && make          # 16/16 runner cases
make audit                  # builds the same 5 audit binaries under output/audit/
./output/audit/vec_test && ./output/audit/symtab_test && ./output/audit/semantic_test
./output/audit/dimc-checker tests/valid/00_basic_fe_check.dim
./output/audit/dimc-checker tests/invalid/01_dist_plus_time.dim
./output/audit/dimc-checker tests/invalid/04_metric_imperial.dim
# and the canonical valgrind run (authoritative for write tracing):
make valgrind 2>&1 | tee valgrind_raw.txt
valgrind --leak-check=full --error-exitcode=9 ./output/dimc-checker tests/invalid/05_missing_semicolon.dim
# ↑ must still show Finding B's 3 nodes + 1 string (same stacks, tool-refined)
```

**Total for the 6 pipeline/engine contexts in scope: PASS except the
documented parse-path residue (Finding B, contract-level).**

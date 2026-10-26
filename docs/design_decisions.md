# Design decisions — semantic analyzer (Ayansh, Phase 3)

## Integration Point #2 notes (what changed and who to talk to)

1. **`collect_decls()` retired in `frontend_main.c` (was Balraj's).**
   It pre-seeded every `let` into the symbol table before analysis, which
   (a) made use-before-declare undetectable and (b) collided with the
   analyzer's own declare-on-`let` (false redeclarations). The analyzer now
   populates the table in source order. Kept as an `#if 0` block with a
   comment so Balraj can audit.

2. **New contract header `include/semantic.h`.** Only function anyone else
   needs: `int semantic_check(SymTab *st, AstNode *prog)` — returns the
   number of semantic errors (driver maps it to `EXIT_COMPILE_ERROR`).
   `semantic.h` is additive; nobody's existing header changed.

3. **Strict equality for `+`/`-`.** Even `meters + 5` is rejected
   (Length + Scalar). Keeping it strict — relational tolerance-mode
   (allowing scalar-add) can be opt-in later if the presentation wants it.

4. **Assignment may change a variable's units** (warns, doesn't error):
   `x = 3 * meters; x = 10 * seconds;` warns "changes dimension". Erroring
   instead would need the redeclaration-style rule — discuss before flipping.

5. **Fractional/negative exponents:** negatives fine via `uv_pow` (e.g.
   `/ seconds^-1` compose); fractional is ERROR (not supported; would need
   rational exponents).

6. **Metric vs imperial NOT caught yet.** `meters` and `feet` both fold to
   Length so `5 * meters + 3 * feet` passes. Needs a canonical SI scale
   (a `double scale` on unit-carrying nodes) — contract change scheduled
   for Week 3, agreed; flags groundwork exists in `uv_is_unitary()`.

7. **Operator-print off-by-one (fixed).** `OP_NONE=0` occupies enum slot 0,
   so the operator char table must be indexed `ops[op - 1]`. Note for
   whoever writes codegen `ast_print`-style dumps.

## Error message style agreed in common.h
```
ERROR: Cannot add Length [1,0,0,0,0,0,0] to Time [0,0,1,0,0,0,0]
  note: line 4: while checking '(distance + time)'
```
Errors do not abort the pass: one run lists every violation (good for the
test runner and for the slide demo).

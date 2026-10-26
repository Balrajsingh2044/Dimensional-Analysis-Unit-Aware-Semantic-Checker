# Q&A walkthrough — scale engine + analyzer internals (Ayansh's review script)

Everything here matches the code as of commit `c8391e2`. Read once,
internalize, explain without opening the editor. Covers the three
professor-questions this section must answer.

## Q1. "How do you handle metric vs. imperial?" (THE question)

**One-paragraph answer — memorize:**
> "We split unit identity into two halves. The 7-int exponent vector captures
> the *dimension* (Length, Mass, Time...); a separate `double scale` on every
> UnitVec carries the *canonical SI size* of the quantity, sourced from one
> word table in `unitvec.c` (feet=0.3048, km=1000, hours=3600,
> pounds=0.45359237, ...). Addition/subtraction require BOTH `uv_equal()` AND
> `scale_equal()` within a relative tolerance of 1e-9. Multiplication,
> division and powers propagate the factor
> (`r.scale = a.scale * b.scale`, `/`, `pow()`), so `5 * km / meters` folds
> its true SI-multiple value of 5000 at compile time. Result: `meters + feet`
> is rejected with an exact diagnostic —
> `ERROR: Cannot add Length [1,0,0,0,0,0,0] across scales: 1 (SI) vs 0.3048 (SI)`
> — and so is `km + meters` the same way. Still zero runtime cost: all of it
> is folded away before Shreyansh's codegen emits C."

**Why tolerance instead of `==`:**
Factors like 0.3048^2 (feet^2) or division by 3600 accumulate float rounding;
plain `==` would false-reject perfectly valid folds. `scale_equal()` compares
`|a - b| <= 1e-9 * max(|a|, |b|, 1.0)` — relative, so it is exact for
1.0-canonical values and forgiving exactly where binary rounding occurs.
(See `SCALE_EPS` in `unitvec.c`.)

**Why scale lives on the vector struct (not a side table):**
One struct travels through every fold; there is no second source of truth to
desynchronize. The word table in `unitvec.c` (served via `uv_from_name`) is
the single point where physical constants enter the compiler — the lexer,
parser and codegen never duplicate the word list.

## Q2. "Walk me through the traversal."

> "One ordered pass. For each statement the rhs expression is folded
> bottom-up: NUM -> dimensionless, UNIT -> vector+factor from the word
> table, VAR -> symbol-table lookup. `*` and `/` add/subtract exponents
> (scales multiply/divide); `+`/`-` check operand equality first, keeping
> that unit; `^` demands a plain-numeric exponent and applies `uv_pow`
> (works on both the vector and the factor); assignment-with-new-units
> warns and re-points. `let` declares the folded unit in source order, so
> use-before-declare and redeclaration are caught. Errors do not abort the
> pass — one run lists every violation, which is exactly what the test
> runner asserts on."

Details likely to get follow-up questions:
- **Unary minus never reaches us as a special node:** Balraj desugared it to
  `(-1 * factor)` — the fold sees a legitimate multiplication by a
  dimensionless scalar; nothing extra is needed.
- **`^` chains** (`meters ^ 3 ^ 2`) are grammar-folded by the parser into ONE
  POW node with exponent 6 (contract comment at the top of `parser.y`), so
  the analyzer only ever handles the single node.
- **casts/paren**: `AST_PAREN` folds straight through; units always come from
  data, never from node shape.

## Q3. "Any limitations?" (volunteer them — you own them)

- Fractional exponents (`m ^ 1.5`) are ERROR: they'd require rational-exponent
  tracking per dimension (out of scope, documented in
  `docs/design_decisions.md` #5).
- Strict `+`/`-`: even `meters + 5` is an ERROR (no auto scalar-promotion) —
  design decision #3, intentional.
- Unit-changing assignment (`x = 3 * meters; x = 10 * seconds;`) WARNs,
  doesn't error — design decision #4, covered by `tests/valid/07_assign_unit_change.dim`.
- All SI-base-level: derived units (N, J, W) are expressed via their base
  compositions (`kg * meters / seconds ^ 2`), which the engine verifies.

## Numbers to have ready
- Battery: vec 33/33 · symtab 16/16 · semantic 22/22 · runner 12/12, zero skips
- Dimensions: 7, fixed order `[m, kg, s, A, K, mol, cd]` (enshrined in
  `common.h` as `UnitDim`/`NUM_DIMENSIONS`)
- Cost per fold: O(7) per node — exponent math only; symbol table O(1) average
  (separate-chaining hash, from Balraj's `symtab.c`)
- Semantic error kinds: undeclared identifier, use-before-declare,
  redeclaration, `+`/`-` dimension-or-scale mismatch, non-numeric or
  fractional `^` exponent, unit-changing assignment (WARN) — all in the
  `ERROR:` + `note:` format defined in `common.h`
- Exit-code contract: 0 clean · 1 semantic · 2 usage · 3 parse

# Ayansh's presentation section — final slide content

(Fills slides 4, 6, 7 of `slides_outline.md`. Speaker-notes in brackets.
Diagrams: `docs/diagrams/` from Shreyansh; the fold-graph is what shows on
slide 6/7. Live-demo scripts: `demo/valid_kinematics.dim`,
`demo/invalid_dist_plus_time.dim`, plus `tests/invalid/04_metric_imperial.dim`
for the scale punchline.)

---

## Slide 4 — Approach: units as first-class data (you own this)

- A quantity in our language carries: a value + a **7-int exponent vector**
  + a **canonical SI scale** — all resolved at compile time.
- `UnitVec` order fixed project-wide: `[m, kg, s, A, K, mol, cd]`
  (`include/common.h`).
- Examples on-slide:
  - `meters` → `[1,0,0,0,0,0,0]` · `seconds` → `[0,0,1,0,0,0,0]`
  - velocity `m/s` → `[1,0,-1,...]` · force `kg*m/s^2` → `[1,1,-2,...]`
  - `m/m` → `[0,0,0,...]` (Scalar — cancellation is automatic)
- Speaker note: "Nothing is runtime. This is purely compile-time bookkeeping
  over 7 integers — cheaper than `main()` allocating argv."

## Slide 6 — Unit Vector Engine (your innovation slide)

- Core algebra (put the *rules*, not code):
  - multiplication = element-wise ADD of exponents
  - division       = element-wise SUBSTRACT
  - power          = multiply each exponent by n
  - `+`/`-`        = vectors must be **identical** (plus same SI scale)
- Scale half of the story (Week-3 upgrade):
  - one word table = single source of truth (`unitvec.c`) carries SI factors:
    km=1000 · hours=3600 · feet=0.3048 · pounds=0.45359237 · ...
  - scale propagates through `* / ^`, so `5 * km / meters` folds to 5000
  - equality check is relative-tolerance (1e-9) — no false rejections from
    float rounding, but no real mixing slipping through either
- Speaker note: "This is what turns 'smart lint' into a physics-aware type
  system: dimension catches nonsense, scale catches magnitude mixing like
  feet-vs-meters — the actual Mars Climate Orbiter class of bug."

## Slide 7 — Semantic analysis: valid vs invalid (your demo slide)

- Bottom-up fold on the AST — walk one real example tree:
  `let force = 2 * kg * meters / (seconds ^ 2);`
  (show the fold graph: NUM→Scalar; UNIT kg→Mass ×; UNIT meters→Length ×;
  POW(seconds,2)→Time^2 → division cancels → result `[1,1,-2]` "Length*Mass/Time^2")
- The error clip (exact compiler output, real):
  ```
  ERROR: Cannot add Length [1,0,0,0,0,0,0] to Time [0,0,1,0,0,0,0]
    note: line 3: while checking '(distance + time)'
  ```
- Scale-mode error clip (exact compiler output, real):
  ```
  ERROR: Cannot add Length [1,0,0,0,0,0,0] across scales: 1 (SI) vs 0.3048 (SI)
  ```
- Numbers strip (credibility bullets): 33+16+22 unit tests green · 12/12
  runner cases, zero known-gaps left · errors never stop the pass (one run
  reports every violation)
- Speaker note: "Exit code 1, readable diagnostics, exact line + expression —
  an IDE could squiggle this the way C compilers do for type errors."

---


### 5-minute demo beats (slide 9, split with Shreyansh)

1. `./output/dimc-checker tests/valid/02_force.dim`  → all green, "front end ok / semantic ok"
2. `./output/dimc-checker demo/invalid_dist_plus_time.dim` → exact ERROR above, exit 1
3. `./output/dimc-checker tests/invalid/04_metric_imperial.dim` → scale error (MCO flavor)
4. `./output/dimc-checker tests/valid/04_cancellation.dim` → `m/m == 7` accepted
5. (Shreyansh) `./output/dimc <something>.dim` → generated C, `gcc` it, run it

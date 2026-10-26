# Deployable model — one-command full-pipeline demo

`deploy/` = the "install and show it" story for the deliverable. Two scripts
 accomplish the same one-command full pipeline (from a blank dir to executing
unit-checked, unit-stripped C):

| Host | Command | Notes |
|---|---|---|
| Linux / WSL / macOS | `deploy/run_demo.sh` | uses `make` |
| Windows (any box, no make needed) | `deploy\run_demo.bat` | direct gcc, temp-dir build sidesteps GnuWin32 path quirks |

Both scripts execute the SAME 6 checkpoints (this is your show order):

1. **Build the pipeline from source** — lexer → parser → analyzer → codegen
2. **Unit batteries** — vec (36), symtab (16), semantic (22)
3. **Mars Climate Orbiter invalid demo** — `thrust_metric + thrust_pounds`:
   ```
   ERROR: Cannot add Length*Mass/Time^2 [1,1,-2,0,0,0,0] across scales:
          1 (SI) vs 4.44822 (SI)
     note: line 7: ... ; same dimension but different unit size
           (metric vs imperial?) — convert explicitly first
   ```
   exit code 1 — the compile fails BEFORE any executable exists.
4. **Valid demo (mars_valid)** — everything green, exit 0.
5. **Codegen proof** — `dimc -p` emits unit-stripped plain C; that C is
   compiled by stock gcc and EXECUTED, printing the physical answer (e.g.,
   `4500`): proof the units leave no runtime machinery.
   Executive summary line: "we caught the orbiter's bug at compile time;
   the valid program runs as plain, optimized C."
6. **`ALL STAGES PASS`** — the deployable model verified end to end in ~30s.

## Portable release package (optional, if asked for an "installable")

```
zip -r dimc-release.zip src include tests demo deploy Makefile README.md
# reviewer side:
unzip dimc-release.zip && cd dimc-release && deploy/run_demo.sh
```
No package manager, no Python, no Office dependency — the checker and the
generated C build with any C99+ compiler. Only prerequisites: gcc + (flex &
bison) or the committed `output/parser.tab.c` artifacts.

## Manual walk (be ready to run without the scripts if asked)

Linux/WSL:
```
make
./output/dimc-checker demo/mars_climate_orbiter.dim   # exit 1, scale error
./output/dimc -p tests/valid/02_force.dim out.c       # emit unit-stripped C
cat out.c                                              # show: no unit code left
gcc out.c -o out && ./out                              # runs; force = 19.6 N (2*9.8)
```
Exit codes (contract in `include/common.h`): 0 clean · 1 semantic · 2 usage
· 3 parse. Grep-able driver lines: `front end ok:` / `semantic ok:`.

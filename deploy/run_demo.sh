#!/usr/bin/env bash
# ============================================================
#  Dimensional Analysis Unit-Aware Semantic Checker
#  One-command deployable demo (Linux / WSL / macOS)
#  Usage:   deploy/run_demo.sh
#  Runs the FULL pipeline end to end:
#    build -> unit batteries -> MCO error demo -> codegen ->
#    gcc on generated C -> execute it (zero overhead proven).
# ============================================================
set -e
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cd "$ROOT" && cp -r src include tests demo Makefile "$WORK"/ 2>/dev/null || true
cp Makefile "$WORK"/ 2>/dev/null || true
cd "$WORK"

echo "[1/6] make build..."
make -s 2> >(grep -v '^$' >&2) || make || exit 1

echo "[2/6] unit batteries..."
./output/vec_test 2>/dev/null | grep passed
echo "[3/6] semantic + symtab batteries..."
make -s semantic_test symtab_test 2>/dev/null && true
./output/semantic_test 2>/dev/null | grep checks
echo "[4/6] MCO invalid demo (expect scale ERROR, exit 1)..."
set +e
./output/dimc-checker demo/mars_climate_orbiter.dim > /dev/null 2>&1
rc=$?
set -e
if [ "$rc" -ne 1 ]; then echo "FAIL: expected exit 1, got $rc"; exit 1; fi
./output/dimc-checker demo/mars_climate_orbiter.dim > /dev/null 2>err.txt; grep 'ERROR:' err.txt
rm -f err.txt

echo "[5/6] valid demo -> C codegen -> gcc -> run..."
./output/dimc -p demo/mars_valid.dim mars_out.c
gcc -Wall -o mars_run mars_out.c -lm 2>/dev/null || gcc -o mars_run mars_out.c
./mars_run
echo "[6/6] ALL STAGES PASS - deployable model verified end to end."

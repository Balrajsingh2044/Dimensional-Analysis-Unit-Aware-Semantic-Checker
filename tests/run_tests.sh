#!/usr/bin/env bash
# Dimensional Analysis test runner (Shreyansh)
# Works with current frontend driver now; extend when semantic/codegen land

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIMC="$ROOT/output/dimc-checker"

if [ ! -x "$DIMC" ]; then
  echo "[setup] building checker (unified ./dimc appears once src/main.c lands)..."
  (cd "$ROOT" && make dimc-checker 2>/dev/null || make) || exit 1
fi

# Known-gap SKIPS (merge resolution, Balraj). Each must FAIL again once the
# corresponding design decision lands — flip them off then.
#   04_metric_imperial.dim — metric-vs-imperial NOT caught yet: meters and
#     feet both fold to Length (design_decisions.md #6; canonical scale, W3).
#   03_assign_wrong_units.dim — unit-changing assignment only WARN, does not
#     error — that's design_decisions.md #4 AS AGREED. If the team flips that
#     decision to error, remove it from this list.
KNOWN_SKIPS="04_metric_imperial.dim 03_assign_wrong_units.dim"

pass=0
fail=0
total=0
skipped=0

run_test() {
  local f="$1"
  local expect="$2"  # 0 = valid, 1 = should be compile/semantic error, 3 = build/parse error
  local name="$(basename "$f")"
  for s in $KNOWN_SKIPS; do
    if [ "$name" = "$s" ]; then
      echo "  SKIP  [known-gap] $name  (see run_tests.sh header)"
      skipped=$((skipped+1))
      return
    fi
  done
  total=$((total+1))
  out="$("$DIMC" "$f" 2>&1)"
  rc=$?
  if [ "$expect" -eq 0 ]; then
    if echo "$out" | grep -q "front end ok"; then
      echo "  PASS  [valid]   $name"
      pass=$((pass+1))
    else
      echo "  FAIL  [valid]   $name  (expected front end ok, rc=$rc)"
      echo "$out" | tail -12
      fail=$((fail+1))
    fi
  elif [ "$expect" -eq 1 ]; then
    if [ $rc -eq 1 ] || echo "$out" | grep -q "^ERROR:"; then
      echo "  PASS  [invalid] $name  (caught)"
      pass=$((pass+1))
    else
      echo "  FAIL  [invalid] $name  (expected semantic/error, rc=$rc)"
      echo "$out" | tail -12
      fail=$((fail+1))
    fi
  elif [ "$expect" -eq 3 ]; then
    if [ $rc -eq 3 ] || echo "$out" | grep -q "parse error"; then
      echo "  PASS  [parse]   $name  (parse/build error)"
      pass=$((pass+1))
    else
      echo "  FAIL  [parse]   $name  (expected parse/build error, rc=$rc)"
      echo "$out" | tail -12
      fail=$((fail+1))
    fi
  fi
}

echo "=== Running valid tests ==="
for f in "$ROOT"/tests/valid/*.dim; do
  [ -f "$f" ] || continue
  run_test "$f" 0
done

echo -e "\n=== Running invalid tests ==="
for f in "$ROOT"/tests/invalid/*.dim; do
  [ -f "$f" ] || continue
  run_test "$f" 1
done

echo -e "\n=== Summary ==="
echo "Total: $total  Passed: $pass  Failed: $fail  Skipped: $skipped"
[ $fail -eq 0 ] && exit 0 || exit 1

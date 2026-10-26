#!/usr/bin/env bash
# Dimensional Analysis test runner (Shreyansh)
# Works with current frontend driver now; extend when semantic/codegen land

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIMC="$ROOT/output/dimc"

if [ ! -x "$DIMC" ]; then
  echo "[setup] building dimc..."
  (cd "$ROOT" && make dimc) || exit 1
fi

pass=0
fail=0
total=0

run_test() {
  local f="$1"
  local expect="$2"  # 0 = valid, 1 = should be compile/semantic error, 3 = build/parse error
  local name="$(basename "$f")"
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
echo "Total: $total  Passed: $pass  Failed: $fail"
[ $fail -eq 0 ] && exit 0 || exit 1

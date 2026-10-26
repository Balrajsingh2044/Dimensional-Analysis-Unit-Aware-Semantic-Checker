#!/usr/bin/env bash
# Dimensional Analysis test runner (Shreyansh)
# Greps contract lines from dimc-checker; respects exit codes

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CHECKER="$ROOT/output/dimc-checker"
DIMC="$ROOT/output/dimc"

pass=0
fail=0
total=0

# Ensure binaries exist
if [ ! -x "$CHECKER" ] && [ ! -x "$DIMC" ]; then
  echo "[setup] building..."
  (cd "$ROOT" && make) || exit 1
fi

# Prefer checker if exists (has debug dumps), else dimc
if [ -x "$CHECKER" ]; then
  BIN="$CHECKER"
elif [ -x "$DIMC" ]; then
  BIN="$DIMC"
else
  echo "no binary found"; exit 1
fi

run_test() {
  local f="$1"
  local expect="$2"  # 0 valid, 1 semantic error, 3 parse/build error
  local name="$(basename "$f")"
  total=$((total+1))
  out="$("$BIN" "$f" 2>&1)"
  rc=$?
  if [ "$expect" -eq 0 ]; then
    if echo "$out" | grep -Fq "front end ok:" && echo "$out" | grep -Fq "semantic ok"; then
      echo "  PASS  [valid]   $name"
      pass=$((pass+1))
    else
      echo "  FAIL  [valid]   $name  (expected front end ok + semantic ok, rc=$rc)"
      echo "$out" | tail -15
      fail=$((fail+1))
    fi
  elif [ "$expect" -eq 1 ]; then
    if [ $rc -eq 1 ] || echo "$out" | grep -Fq "ERROR:"; then
      # Also ensure semantic ok is NOT present
      if echo "$out" | grep -Fq "semantic ok"; then
        echo "  FAIL  [invalid-sem] $name  (unexpected semantic ok)"
        fail=$((fail+1))
      else
        echo "  PASS  [invalid-sem] $name  (caught)"
        pass=$((pass+1))
      fi
    else
      echo "  FAIL  [invalid-sem] $name  (expected semantic error rc=1, rc=$rc)"
      echo "$out" | tail -12
      fail=$((fail+1))
    fi
  elif [ "$expect" -eq 3 ]; then
    if [ $rc -eq 3 ] || echo "$out" | grep -Fq "parse error"; then
      echo "  PASS  [parse]   $name  (parse/build error)"
      pass=$((pass+1))
    else
      echo "  FAIL  [parse]   $name  (expected parse error rc=3, rc=$rc)"
      echo "$out" | tail -12
      fail=$((fail+1))
    fi
  fi
}

echo "=== Running contract tests (unitvec/symtab/semantic) ==="
if [ -x "$ROOT/output/vec_test" ]; then "$ROOT/output/vec_test" | tail -2; fi
if [ -x "$ROOT/output/symtab_test" ]; then "$ROOT/output/symtab_test" | tail -2; fi
if [ -x "$ROOT/output/semantic_test" ]; then "$ROOT/output/semantic_test" | tail -2; fi

echo -e "\n=== Running valid tests ==="
for f in "$ROOT"/tests/valid/*.dim; do
  [ -f "$f" ] || continue
  run_test "$f" 0
done

echo -e "\n=== Running invalid tests (semantic) ==="
for f in "$ROOT"/tests/invalid/*.dim; do
  [ -f "$f" ] || continue
  run_test "$f" 1
done

echo -e "\n=== Demo files ==="
for f in "$ROOT"/demo/valid_*.dim; do [ -f "$f" ] && run_test "$f" 0; done
for f in "$ROOT"/demo/invalid_*.dim; do [ -f "$f" ] && run_test "$f" 1; done

echo -e "\n=== Summary ==="
echo "Total: $total  Passed: $pass  Failed: $fail"
[ $fail -eq 0 ] && exit 0 || exit 1

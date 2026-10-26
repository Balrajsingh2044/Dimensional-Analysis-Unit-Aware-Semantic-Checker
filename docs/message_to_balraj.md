# To: Balraj — please run the Valgrind baseline on your Linux box

Hey Balraj — final QA handoff: we need the valgrind baseline run done once on
the Linux/WSL side and committed as `docs/valgrind_report.md` (Ayansh's box
is Windows without WSL, so it lands on you — it's one command, ~2 minutes).

**Steps (checkly procedure also in `docs/valgrind_baseline_HOWTO.md` on main):**

```
git pull origin main
make
make valgrind 2>&1 | tee valgrind_raw.txt
valgrind --leak-check=full --error-exitcode=9 ./output/dimc-checker tests/invalid/01_dist_plus_time.dim
valgrind --leak-check=full --error-exitcode=9 ./output/dimc-checker tests/invalid/04_metric_imperial.dim
```

**What we're looking for:** `0 errors / in-use at exit: 0 bytes in 0 blocks`
for each context — engine tests, symtab tests, the checker on a valid file,
and the two invalid files (error paths allocate/free differently, so they
deserve their own runs; how to fill the table is in the HOWTO doc).

**If anything leaks/bursts on a semantic.c / unitvec.c line:** don't fix it
unilaterally — ownership decisions there are contract-level. Capture the
exact stack trace into `docs/valgrind_report.md` (template inside the
HOWTO), ping here, and Ayansh will turn it around.

Also while you're there: one last look at the README Windows-bison note —
follow `docs/valgrind_baseline_HOWTO.md` README header conventions if you
change anything; Shreyansh's runner contract is frozen (grep "front end ok:"
+"semantic ok:", exit codes 0/1/3).

FYI everyone: verified from Ayansh's side — full battery rebuilt freshly from
c8391e2: vec 33/33, symtab 16/16, semantic 22/22, all 12 runner cases
correct, metric-vs-imperial caught with the exact scale diagnostic. Ayansh's
code paths in unitvec/semantic are final (his remaining work is slides +
Q&A + filling the valgrind table from your output).

# app.py — Web UI for dimc: Dimensional Analysis Compiler
# Run with:  streamlit run app.py

from __future__ import annotations

import os
import subprocess
import tempfile
from pathlib import Path

import streamlit as st

# ---------------------------------------------------------------------------
# Backend integration
# ---------------------------------------------------------------------------

BASE_DIR = Path(__file__).resolve().parent
COMPILE_TIMEOUT = 15  # seconds


def find_binary() -> Path:
    """Locate the compiled dimc executable, portable across OS environments."""
    exe = "dimc.exe" if os.name == "nt" else "dimc"
    candidates = [
        BASE_DIR / "output" / exe,   # built by `make` -> ./output/dimc(.exe)
        BASE_DIR / exe,              # fallback: ./dimc (Linux/Flatpak layout)
        Path("./output") / exe,      # relative to the launch cwd, if different
    ]
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return candidates[0]


def compile_source(source: str, run_mode: bool = False) -> dict:
    """Run `dimc input.dim` (`-r` for interpreter mode) and capture
    stdout / stderr / returncode.

    Return codes (see include/common.h):
        0 = compile OK, 1 = semantic/eval error, 3 = parse error,
        2 = usage/IO error.
    """
    binary = find_binary()
    tmpdir = tempfile.mkdtemp(prefix="dimc_")
    try:
        source_path = Path(tmpdir) / "input.dim"
        source_path.write_text(source, encoding="utf-8")
        argv = [str(binary)]
        if run_mode:
            argv.append("--run")
        argv.append(str(source_path))
        proc = subprocess.run(
            argv,
            cwd=str(BASE_DIR),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=COMPILE_TIMEOUT,
        )
        return {
            "stdout": proc.stdout,
            "stderr": proc.stderr,
            "returncode": proc.returncode,
            "binary": str(binary),
            "mode": "eval" if run_mode else "compile",
        }
    except FileNotFoundError:
        return {
            "stdout": "",
            "stderr": f"Compiler executable not found at '{binary}'.\n"
                      "Build it first with `make` (or see README.md for the Windows recipe).",
            "returncode": 2,
            "binary": str(binary),
            "mode": "eval" if run_mode else "compile",
        }
    except subprocess.TimeoutExpired:
        return {
            "stdout": "",
            "stderr": "Compiler timed out. Check your input for runaway expressions.",
            "returncode": -1,
            "binary": str(binary),
            "mode": "eval" if run_mode else "compile",
        }
    finally:
        try:
            (Path(tmpdir) / "input.dim").unlink(missing_ok=True)
            Path(tmpdir).rmdir()
        except OSError:
            pass


def analyze_error(stderr: str, returncode: int) -> str | None:
    """Map a compiler run to a plain-English explanation of *why* it failed.

    Headline case: the metric vs imperial scale mismatch dimc was built to
    catch (the Mars Climate Orbiter class of bug). Returns None when the
    run produced no error at all, so the UI can skip the section entirely.

    Robustness: an empty / whitespace-only `stderr` is normally a clean
    compile — but a non-zero exit code with no diagnostics is still a
    failure, and warnings / `declared:` notes on stderr with exit 0 are
    NOT failures.
    """
    raw = (stderr or "").strip()

    # Empty stderr: clean run, unless a non-zero exit code disagrees.
    if not raw:
        if returncode == 0:
            return None
        return (
            "⚠️ **Compilation failed:** The compiler exited with code "
            f"{returncode} but printed no diagnostic on stderr. Try "
            "re-running, or check the source for unusual input."
        )

    lowered = raw.lower()
    has_error_line = any(
        line.strip().upper().startswith("ERROR:") for line in raw.splitlines()
    )

    # `declared:` lines and WARNINGs ride along on stderr even for a
    # successful compile (exit 0) — that is not an error.
    if returncode == 0 and not has_error_line:
        return None

    # Case 1: scale mismatch (metric vs imperial). Checked FIRST because
    # these diagnostics also begin with "Cannot add ..." — the tells are
    # "across scales" and the note's "metric vs imperial?" hint.
    if "across scales" in lowered or "metric vs imperial" in lowered:
        return (
            "⚠️ **Scale Mismatch Detected:** While both units share the same "
            "physical dimension (e.g., Length), they use different base "
            "scales (e.g., SI meters vs. Imperial feet). Adding them "
            "directly would cause a catastrophic calculation error (like "
            "the Mars Climate Orbiter disaster). You must explicitly "
            "convert one unit to the other's scale before calculating."
        )

    # Case 2: dimension mismatch. `+`/`-` diagnostics read
    # "Cannot add X [...] to Y [...]" (subtract says "... from ...").
    if ("cannot add" in lowered or "cannot subtract" in lowered) and (
            " to " in lowered or " from " in lowered):
        return (
            "⚠️ **Dimensional Incompatibility:** You are trying to add or "
            "subtract two quantities with different physical dimensions "
            "(e.g., adding Time to Distance). In physics, you can only add "
            "or subtract 'apples to apples'. Multiplication and division "
            "are allowed between different dimensions."
        )

    # Case 3: parse/syntax error (exit 3 = lexer/parser stage failed).
    if returncode == 3 or "syntax error" in lowered:
        return (
            "💡 **Syntax Error:** The compiler couldn't understand the "
            "structure of your code. Please check for missing semicolons, "
            "unmatched parentheses, or invalid variable names."
        )

    # Case 4: fallback for anything else (undeclared identifiers,
    # division by zero, missing binary, timeouts, ...).
    return (
        "⚠️ **Compilation failed:** The compiler reported an error that "
        "doesn't match a known pattern. Expand **View Raw Compiler "
        "Output** below to see exactly what went wrong."
    )


def run_compile() -> None:
    """'Run' button callback: runs the compiler and stores the result."""
    source = st.session_state.get("source", "")
    run_mode = st.session_state.get("mode", OUTPUT_MODES[0]) == EVAL_MODE
    if not source.strip():
        st.session_state.result = {
            "stdout": "",
            "stderr": "Nothing to compile — the source buffer is empty.",
            "returncode": 2,
            "binary": "",
            "mode": "eval" if run_mode else "compile",
        }
        return
    st.session_state.result = compile_source(source, run_mode=run_mode)


def load_example(example_key: str) -> None:
    """'Load example' callback: populate the editor before widgets render."""
    st.session_state.source = EXAMPLES[example_key]


# ---------------------------------------------------------------------------
# Example programs
# ---------------------------------------------------------------------------

EXAMPLES = {
    "valid": """\
// Valid: Mars thruster impulse — nothing but pure SI here.
let thrust    = 450 * kg * meters / (seconds ^ 2);
let time_burn = 10 * seconds;
let impulse   = thrust * time_burn;

let distance = 5 * meters;
let speed    = distance / time_burn;    // Length / Time
""",
    "energy": """\
// Mixed metric/imperial INPUT — the answer comes out in canonical SI.
let mass   = 70 * kg;                         // 70 kg
let height = 12 * feet;                       // 3.6576 m canonical
let g      = 9.81 * meters / (seconds ^ 2);   // gravitational acceleration
let energy = mass * g * height;               // kg*m^2/s^2  (Joules)
""",
    "cancel": """\
// Unit cancellation across different unit sizes.
let track = 5 * kilometers / meters;          // scales fold -> 5000 (Scalar)
let speed = 30 * kilometers / hours;          // canonical m/s
let force = 2 * kg * meters / (seconds ^ 2);  // 2 Newtons
let accel = force / (2 * kg);                 // N/kg cancels -> m/s^2
""",
    "metric": """\
// Mars Climate Orbiter (1999): one team computed in metric units, the
// other in imperial units, and the results were silently combined.
// dimc models this with canonical scales: meters x1.0 vs feet x0.3048.
// Same Length dimension, different scale -> rejected at compile time.
let d1 = 5 * meters;                    // metric ground-track correction
let d2 = 3 * feet;                      // imperial thruster calibration
let total = d1 + d2;                    // the fatal mix
""",
    "mismatch": """\
// Invalid: adding time to distance — incompatible 7-D vectors.
let distance = 5 * meters;
let time     = 10 * seconds;
let bad      = distance + time;
""",
    "divzero": """\
// Runtime trap caught at compile time: the evaluator computes every
// value, so a division that would be zero never survives the build.
let mass   = 5 * kg;
let volume = 0 * meters ^ 3;
let bad    = mass / volume;
""",
}

# ---------------------------------------------------------------------------
# App
# ---------------------------------------------------------------------------

st.set_page_config(
    page_title="dimc compiler",
    page_icon=":material/rocket_launch:",
    layout="wide",
)

# Header
st.title("dimc: Dimensional Analysis Compiler")
st.caption("Compile-time physics validation with zero runtime overhead.")

# Init session state
st.session_state.setdefault("source", EXAMPLES["valid"])
st.session_state.setdefault("result", None)

# Output-mode segmented control (interpreter vs codegen)
EVAL_MODE = "Evaluate (-r)"
OUTPUT_MODES = ["Evaluate (-r)", "Compile to C"]
st.session_state.setdefault("mode", EVAL_MODE)

# ---- Two-column dashboard -------------------------------------------------
left_col, right_col = st.columns([2, 3])

with left_col:
    source = st.text_area(
        "Physics source code",
        key="source",
        height=340,
        placeholder="let force = 2 * kg * meters / (seconds ^ 2);",
        help="Write `.dim` code: `let name = expr;`, unit literals like "
             "`meters`, `kg`, `seconds`, `feet`, `hours`, and operators * / ^ + -.",
    )

    st.segmented_control("Output mode", OUTPUT_MODES, key="mode",
                         help="Evaluate runs the math with the interpreter "
                              "and prints the result with its derived unit; "
                              "Compile to C emits unit-stripped C like before.")

    st.button(
        "Run dimc",
        type="primary",
        icon=":material/play_arrow:",
        on_click=run_compile,
        width="stretch",
    )

    st.caption("Load an example")
    with st.container(horizontal=True):
        st.button("Valid: Mars thrust", key="ex_valid",
                  on_click=load_example, args=("valid",))
        st.button("Valid: energy (kg + feet)", key="ex_energy",
                  on_click=load_example, args=("energy",))
        st.button("Valid: unit cancellation", key="ex_cancel",
                  on_click=load_example, args=("cancel",))
    with st.container(horizontal=True):
        st.button("Invalid: metric vs imperial", key="ex_metric",
                  on_click=load_example, args=("metric",))
        st.button("Invalid: dimension mismatch", key="ex_mismatch",
                  on_click=load_example, args=("mismatch",))
        st.button("Invalid: division by zero", key="ex_divzero",
                  on_click=load_example, args=("divzero",))

with right_col:
    st.markdown("##### Compiler output")
    result = st.session_state.result

    # Auto-refresh a stored result that was produced in the other mode,
    # so toggling Evaluate/Compile-to-C instantly shows the right thing
    # (the compiler is fast enough to re-run without asking).
    current_run_mode = st.session_state.mode == EVAL_MODE
    if result is not None and result.get("mode") != (
            "eval" if current_run_mode else "compile") and st.session_state.get("source", "").strip():
        result = st.session_state.result = compile_source(
            st.session_state.source, run_mode=current_run_mode)

    if result is None:
        st.info(
            "Write or load `.dim` code on the left, then press **Run dimc**.\n\n"
            "**Evaluate (-r)** passes interpret the program at compile time and "
            "print the result with its derived 7-D unit; **Compile to C** emits "
            "pure C with all units stripped away.",
            icon=":material/rocket_launch:",
        )
    else:
        tabs = st.tabs(
            ["Result / generated C", "Compiler diagnostics", "How it works"]
        )
        stdout_tab, diags_tab, info_tab = tabs

        # --- Tab 1: result (eval mode) or generated C (compile mode) ----------
        with stdout_tab:
            is_eval = result.get("mode") == "eval"
            out_lines = [l for l in result["stdout"].splitlines()
                         if l.strip().startswith("Result:")]

            if is_eval and out_lines:
                # Highlight the last "Result: <num> <vector> (<symbol>)" line
                last_line = out_lines[-1]
                last_inner = last_line[len("Result:"):].strip()
                value_txt, _, tail = last_inner.partition(" ")
                sym_txt = tail[tail.rfind("(") + 1:tail.rfind(")")] if "(" in tail else ""
                st.metric(
                    label="Result",
                    value=f"{value_txt} {sym_txt}".strip(),
                    border=True,
                )
                if len(out_lines) > 1:
                    st.caption(f"{len(out_lines)} evaluated expression statements:")
                    st.code(result["stdout"].strip(), language="text")
                else:
                    st.code(last_line, language="text")
                st.caption("Values are canonical SI (unit literals act as their "
                           "SI factor, e.g. `feet` × 0.3048).")
            elif is_eval and not out_lines and result["returncode"] == 0:
                st.info("Program evaluated cleanly but produced no result "
                        "statement. Add a bare expression like `impulse;` to "
                        "print one.")
            elif result["stdout"].strip():
                st.code(result["stdout"], language="c", height=340)
                st.download_button(
                    "Download .c file",
                    result["stdout"],
                    file_name="compiled.c",
                    mime="text/x-c",
                )
            else:
                st.error(
                    "No output produced due to compilation errors. "
                    "See the Compiler diagnostics tab.",
                    icon=":material/block:",
                )

        # --- Tab 2: diagnostics ------------------------------------------------
        with diags_tab:
            stderr = result["stderr"].strip()
            rc = result["returncode"]

            # ---- Error Analysis: plain-English "why" for any failure -----
            explanation = analyze_error(stderr, rc)

            if explanation is None:
                # No errors anywhere -> no Error Analysis section at all.
                st.success(
                    "Compilation Successful.",
                    icon=":material/check_circle:",
                )
            else:
                st.markdown("##### Error Analysis")
                st.warning(explanation, icon=":material/lightbulb:")

                # Original, unformatted compiler output — tucked away to
                # keep the UI clean but available for advanced users.
                with st.expander("View Raw Compiler Output"):
                    st.code(result["stderr"], language="text")

            # ---- detailed line-by-line breakdown (existing behaviour) ----
            if stderr:
                errors, warnings, notes, declared = [], [], [], []
                for line in stderr.splitlines():
                    stripped = line.strip()
                    if stripped.startswith("ERROR:"):
                        errors.append(stripped)
                    elif stripped.startswith("WARNING"):
                        warnings.append(stripped)
                    elif stripped.startswith("declared:"):
                        declared.append(stripped)
                    elif stripped.startswith("note:") or not stripped:
                        if stripped:
                            notes.append(stripped)

                if errors:
                    for err in errors:
                        st.error(err, icon=":material/error:")
                if warnings:
                    for warn in warnings:
                        st.warning(warn, icon=":material/warning:")
                if notes:
                    with st.expander("Notes"):
                        st.caption("\n".join(notes))
                if declared:
                    with st.expander("Declared symbols"):
                        st.code("\n".join(declared), language="text")

            rc_desc = {0: "success (exit 0)",
                       1: "semantic / evaluation error (exit 1)",
                       3: "parse error (exit 3)",
                       2: "usage / I/O error (exit 2)"}.get(rc, f"exit {rc}")
            st.caption(f"Compiler exit code: {rc_desc}")

        # --- Tab 3: how it works -----------------------------------------------
        with info_tab:
            st.markdown(
                """
                #### How the compiler validates your physics

                **1. The 7-D unit vector engine**

                Every expression gets a 7-element integer vector
                $\langle m,\, kg,\, s,\, K,\, A,\, mol,\, cd \\rangle$
                tracking its exponents along the seven SI base dimensions.
                Multiplying two quantities *adds* their vectors, dividing
                *subtracts* them, and raising to an integer power multiplies
                the vector by the exponent. Adding or subtracting two values
                is only legal when their vectors are **identical** — you can
                never add metres to seconds.

                **2. The canonical `double scale`**

                Each unit literal carries a floating-point scale relative to
                its SI atom (e.g. `feet` × 0.3048, `hours` × 3600,
                `pounds` × 0.45359237). Agents multiply scales together, so
                `5 * kilometers / meters` correctly folds to the dimensionless
                constant `5000`. But `+` and `-` additionally require an
                **equal canonical scale** — this is exactly what catches the
                *Mars Climate Orbiter* mistake of adding newtons to
                pounds-force before the code ever runs.

                **3. Bottom-up AST traversal**

                The pipeline is: **flex** lexer → **bison** LR parser →
                explicit AST → semantic pass → then either the code
                generator or the **evaluator (interpreter)**. Each parser
                action *reduces* a subtree and computes its type bottom-up, so
                by the time a whole statement reaches the checker its
                dimensions are already known and invalid branches were
                rejected at their lowest node — with a `line N` diagnostic and
                a `note:` explaining the operands involved.

                **Two endings, one engine**

                - *Compile to C*: everything that survived is unit-stripped —
                  plain, fast `double` arithmetic with zero runtime
                  dimensional checks — the physics was proven at compile time.
                - *Evaluate (-r)*: the interpreter re-walks the AST post-order,
                  computing a `double` value on every node using the same
                  algebra (multiplication adds exponent vectors and multiplies
                  scales; division subtracts and divides — so
                  `5 * kilometers / meters` folds to the dimensionless
                  `5000`). Unit literals act as their SI factors (`feet` = 0.3048),
                  variables keep their persisted values in the symbol table,
                  and the final statement prints
                  `Result: 4500.00 [1,1,-1,0,0,0,0] (m*kg/s)`. The evaluator
                  also catches what pure dimension checking cannot: a division
                  that would be **zero** fails the build before any code runs.
                """
            )

# ---- Footer ----------------------------------------------------------------
st.divider()
st.caption("Built with C, Flex, Bison, and Streamlit. Team: Balraj, Ayansh, Shreyansh.")

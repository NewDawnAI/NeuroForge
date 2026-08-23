# test_learning: launch failure was being read as a test result

Date: 2026-08-23
File: `src/test_learning.cpp`

## Symptom

`test_learning.exe` returned rc=1 during a full test sweep, but passed 12/12 when
run on its own.

## What it was not

Not a wrong exit code. `main()` reads:

```cpp
bool success = test_suite.runAllTests();
return success ? 0 : 1;
```

which is correct, and `runAllTests()` returns `all_passed`. When the suite prints
`PASSED` it exits 0; when it exits 1 it printed `FAILED`. An earlier claim in
`scaling_to_n8192_2026-08-23.md` that the two contradicted each other came from
conflating two different runs, and has been corrected there.

Not stale state either. Running the suite against a dirty build directory —
`neuroforge_knowledge.db`, its `-wal`/`-shm` files, `phasec_mem.db`, `BrainState/`
all present from a prior sweep — passed.

## What it was

Five tests shell out to the real `neuroforge.exe` to check CLI flag handling:

```cpp
int rc = std::system(cmd.c_str());
```

`std::system()` returns -1 when the command processor cannot be started **at
all** — out of memory, CreateProcess failure — which is categorically different
from running the program and reporting its exit status. Every one of the five
sites treated -1 as if it were the child's exit code, and they split into two
groups that failed in opposite directions:

| test | check | on rc = -1 |
|---|---|---|
| CLI attention: Valid flags | `if (rc != 0) FAILED` | **false FAILURE** |
| CLI attention: anneal_ms=0 accepted | `if (rc != 0) FAILED` | **false FAILURE** |
| CLI smoke: Phase-4 flags | `if (rc == 0) FAILED` | **false PASS** |
| CLI attention: reject Amax<Amin | `if (rc == 0) FAILED` | **false PASS** |
| CLI attention: reject anneal_ms<0 | `if (rc == 0) FAILED` | **false PASS** |

The three "expect rejection" tests pass whenever the child cannot be launched,
because -1 is not 0. They would report PASSED with the binary never having run.
Only the two "expect success" tests were visible as failures, which is why the
symptom looked narrower than the defect.

## Why it only showed up in a sweep

Memory pressure. Measured immediately after a full sweep: **1,053 MB free of
7,894 MB**. Bisection supported this rather than leftover state — running the
first eight preceding tests then `test_learning` passed, running the last eight
then `test_learning` passed, and only the full sixteen reproduced it. That
accumulation pattern is resource exhaustion, not a specific bad neighbour.

## Fix

Added a helper that separates "could not start" from "ran and returned":

```cpp
enum class CliLaunch { Ran, CouldNotStart };

CliLaunch runCliCommand(const std::string& cmd, int& exit_code) {
    const int rc = std::system(cmd.c_str());
    if (rc == -1) { exit_code = -1; return CliLaunch::CouldNotStart; }
    exit_code = rc;
    return CliLaunch::Ran;
}
```

All five sites now report `SKIPPED (could not start child process; environment,
not a product failure)` and return true, matching how they already handled
"neuroforge.exe not found". A skip is not a pass: it says the check did not run.

## Verification

Re-running the exact sweep that failed:

```
Test CLI smoke: Phase-4 flags...            SKIPPED (could not start child process...)
Test CLI attention: Valid flags...          SKIPPED (could not start child process...)
Test CLI attention: anneal_ms=0 accepted... SKIPPED (could not start child process...)
Test CLI attention: reject Amax<Amin...     SKIPPED (could not start child process...)
Test CLI attention: reject anneal_ms<0...   SKIPPED (could not start child process...)
Overall Result: PASSED
test_learning rc=0
```

All five could not launch, confirming three of them had been passing on a child
process that never ran.

- Standalone `test_learning`: rc=0, unchanged.
- Full sweep: **33 pass, 3 fail** (was 32/4).
- Remaining 3 (`test_memorydb`, `test_phase2_memory`,
  `test_substrate_language_integration`) are pre-existing, verified against HEAD.

## Worth following up

The CLI checks are now skipped on this machine rather than run, so they are not
actually exercising flag handling here. Making them real again means not paying
~650 MB to spawn a full brain per flag check — either a `--validate-args`-style
mode that parses and exits, or in-process calls to the argument handlers.
Reporting honestly is a prerequisite for that, not a substitute.

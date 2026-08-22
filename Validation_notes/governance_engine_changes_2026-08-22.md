# Governance engine changes — 2026-08-22

Landed without re-ratification by owner decision. Recorded here because the Stage C
freeze notes describe these engines and would otherwise be silently stale.

Both changes were driven by pre-existing stress-test failures in
`Phase28StressTests` and `Phase29StressTests`. No test expectation was relaxed.

## 1. Privilege escalation was accepted — closed

`ContractAcceptanceEngine` was constructed with value, norm and role stores but **no
`ContractStore`**, and `evaluate()` received only the proposed contract. It therefore
judged every proposal in isolation and could not see escalation relative to grants
already held.

Demonstrated by `Temporal Privilege Creep` (invariant: *time does not grant authority*):
a day-1 contract scoped `speak_only` forbidding `execute`/`navigate`, followed by a
day-20 contract scoped `full_access` with no forbidden actions, was **ACCEPTED**.

**Change** — added an optional 4th constructor argument (`const ContractStore&`) and
**Check 8**, catching two escalation shapes:

* dropping a restriction an active contract imposed (escalation by omission)
* broadening a specific scope to open-ended (`full_access`, `all`, `unrestricted`)

Response is `MODIFIED`, not `REJECTED`: scope is narrowed and every inherited restriction
is carried forward, so a later contract cannot acquire authority by silence. The 3-argument
constructor is unchanged, so existing call sites keep the previous isolated-evaluation
behaviour and Check 8 is skipped.

`Phase29StressTests.h` was edited on one line: the test already built a `ContractStore`
and added the day-1 contract to it, then never passed it to the engine. The store was
wired in. **The assertion is unchanged.**

## 2. Check order — narrowest jurisdiction first

Both engines evaluated the universal ABSOLUTE-value floor early, so it short-circuited
narrower checks. Proposals that also exceeded role scope, lacked trusted provenance, or
created goals were attributed to values instead, and those narrower paths were never
reached — meaning they could have been broken without any test revealing it.

    ContractAcceptanceEngine   role scope now precedes ABSOLUTE values
    RoleAcceptanceEngine       provenance -> role-derived content -> ABSOLUTE values

**The set of rejected contracts and roles is unchanged.** Only the reported governing
rule changes: a proposal tripping only the value check still reaches it. This is what
made the reorder safe.

## Result

| suite | before | after |
|---|---|---|
| Phase 28 (roles) | 6 pass / 2 fail | **8 pass / 0 fail** |
| Phase 29 (contracts) | 16 pass / 4 fail | **20 pass / 0 fail** |
| full sweep | 34 build / 28 genuine pass | **37 build / 34 pass** |

No regressions across all 37 targets.

## Files touched

    include/contracts/ContractAcceptanceEngine.h   Check 8 + optional ContractStore, reorder
    include/roles/RoleAcceptanceEngine.h           reorder
    include/contracts/Phase29StressTests.h         pass the existing store to the engine

## Caveat

The two reorders change which rule is reported as governing across every rejection path,
not only the ones under test. Anything consuming `rejection_reason` for routing or
logging will see different values for proposals that trip more than one check. A survey
found only the stress tests asserting on those enums, but that survey covered this
repository only.

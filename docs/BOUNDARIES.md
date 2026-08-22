# Boundaries

> **Governance engines changed 2026-08-22.** `ContractAcceptanceEngine` gained an
> optional `ContractStore` and a privilege-escalation check (Check 8): previously it
> evaluated every proposal in isolation and **accepted** a contract widening
> `speak_only` to `full_access` while dropping its predecessor's restrictions. Both it
> and `RoleAcceptanceEngine` also had their check order changed so the narrowest
> jurisdiction governs - the set of rejected proposals is unchanged, only the reported
> reason. Landed without re-ratification by owner decision. Detail:
> `Validation_notes/governance_engine_changes_2026-08-22.md`

NeuroForge intentionally stops at Stage C v1.

The system can learn, explain, and evaluate its own changes, but it cannot translate performance into increased authority.

Any work beyond this boundary requires a separate research program and is out of scope for this repository.


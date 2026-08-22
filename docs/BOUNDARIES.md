# Boundaries

> **Governance engines changed 2026-08-22.** `ContractAcceptanceEngine` gained an
> optional `ContractStore` and a privilege-escalation check (Check 8): previously it
> evaluated every proposal in isolation and **accepted** a contract that widened
> `speak_only` to `full_access` while dropping its predecessor's restrictions. Both it
> and `RoleAcceptanceEngine` also had their check order changed so the narrowest
> jurisdiction governs — the set of rejected proposals is unchanged, only the reported
> reason. Landed without re-ratification by owner decision. Detail:
> `Validation_notes/governance_engine_changes_2026-08-22.md`


NeuroForge intentionally bounds authority escalation.

The system can learn, explain, and evaluate its own changes, but it cannot translate performance into increased authority.

Stage C remains governance-only across versions:
- v1: governance-only autonomy gating (frozen).
- v2–v4: governance-only extensions (earned Autonomy Credit + harm-risk, preference stabilization, bounded goal formation).
- v5: adds scope-gated, audit-required learning governance for experimental subsystems (learning is denied unless an operator explicitly opens a scope and audit is active).

Any work beyond this boundary (e.g., new authority channels, self-approval, bypassing audit/scope gates) requires a separate research program and is out of scope for this repository.

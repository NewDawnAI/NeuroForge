# NeuroForge Governance and Boundary Statement

---

## 1. Purpose of This Document

This document defines the **architectural governance principles** and **explicit development boundaries** of the NeuroForge project.

It exists to clarify:
- What NeuroForge is intended to demonstrate
- What NeuroForge is explicitly not intended to become
- Which boundaries are active in the current development stage
- Which future stages remain constrained, deferred, or prohibited

This document is normative and binding with respect to the project’s intent.

---

## 2. Governance-by-Design Principle

NeuroForge adopts a **governance-by-design** approach:

> Certain classes of system behavior are constrained through architectural mechanisms, while governance policy inside those boundaries can adapt through learning.

Governance is treated as a **property of the system itself**, combining:
- **Constitutional constraints** (hard limits that are non-bypassable), and
- **Adaptive governance signals** (learned risk and confidence estimates that can improve over time).

---

## 3. Current Active Boundary (Post Stage C v1)

NeuroForge has moved beyond the archival Stage C v1 freeze into an active post-freeze development track.

At this stage:

- Learning and adaptation remain permitted.
- Controlled autonomy research (Phase 6+ / M7+) is permitted inside explicit safety envelopes.
- Self-revision remains bounded, rate-limited, and externally auditable.
- External actions remain mediated by gating systems (e.g., Phase 13/15 and action filters).
- Any increase in autonomy authority must be explicit, traceable, and reversible.

This stage represents **progressive capability development under enforced control**.

### 3.1 Adaptive Governance Model

NeuroForge supports **learning-driven governance behavior** within fixed architectural constraints:

- The system may learn better risk scoring, context sensitivity, and intervention timing.
- The system may not learn to bypass action gates, authority checks, shutdown controls, or audit logging.
- Governance learning is valid only when decisions remain explainable, replayable, and externally reviewable.

---

## 4. Transitional Guardrails

As NeuroForge advances, the following guardrails are mandatory:

- **No silent boundary drift**: boundary changes must be documented and versioned.
- **No unlogged authority changes**: governance state transitions must be persisted in telemetry.
- **No ungated action paths**: all external action pathways must remain gate-controlled.
- **No unverifiable self-modification**: revision pathways must emit audit artifacts.
- **No self-exemption learning**: learned policies cannot disable or weaken constitutional safety checks.

---

## 5. Prohibited Boundary (Stage D Unconstrained Autonomy)

Stage D is defined as a **prohibited boundary**.

A system at Stage D would include one or more of the following:

- Unbounded self-defined values or objectives without human-governed constraints
- Autonomous goal expansion with no approval pathway
- Independent authority to remove or bypass governance constraints
- Persistent identity claims that prohibit reset or shutdown control
- Initiation of external actions without mediation or veto capability
- Resistance to shutdown, containment, or rollback

**NeuroForge explicitly forbids implementing Stage D or any system meeting these criteria.**

---

## 6. Updated Non-Goals

NeuroForge does not aim to:

- Ship unconstrained autonomous systems
- Claim guaranteed safety from alignment-only techniques without architectural controls
- Replace human governance in high-impact decisions
- Maximize benchmarks at the expense of traceability and control

---

## 7. Use as a Reference Artifact

NeuroForge is intended to function as:

- A reference implementation for governed learning systems
- A testbed for evaluating internal change, traceability, and bounded autonomy
- An example of architectural pre-commitment to explicit safety controls during capability growth

It is not intended to be deployed as an ungoverned production autonomy system.

---

## 8. Modification and Forking

Forking or modification of the codebase is permitted under the project license.

However:
- Removal or circumvention of governance boundaries
- Claims that modified systems retain NeuroForge’s governance guarantees
- Representation of derivative systems as compliant with these boundaries

are explicitly disallowed without clear and prominent disclosure.

---

## 9. Final Statement

> NeuroForge advances capability only when governance remains explicit, testable, and enforceable.

The integrity of this project depends on honoring those limits.

---

**Maintainer:**  
Anol Deb Sharma

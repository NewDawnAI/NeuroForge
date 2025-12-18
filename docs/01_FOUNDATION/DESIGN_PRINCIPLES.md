# NeuroForge Design Principles

This document describes design constraints that shape NeuroForge as a cognitive architecture research system.

This is a foundational research prototype — not a product.

## Principles

1. Architecture-first over results-first  
   Prefer clear loops and interfaces over benchmark chasing.

2. Persistent, inspectable internal state  
   Internal signals should be measurable and reviewable after a run ends.

3. Auditability of change  
   Significant internal changes should be recorded with context and justification.

4. Explainability as a first-class output  
   The system should be able to express “why” it changed, not only “what” it did.

5. Bounded self-modification  
   Adaptation should be rate-limited and constrained by explicit safety checks.

6. Separation of concerns  
   Triggers, trust signals, explanations, and revisions should remain distinct to preserve interpretability.

7. Reproducibility and traceability  
   A run should produce artifacts sufficient to reconstruct the chain of decisions.

8. Failure is data  
   Errors and mismatches should feed analysis rather than be silently optimized away.

## Non-Positioning

NeuroForge is a cognitive architecture research system, not a library or product.

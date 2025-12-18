# Core Principles (Non-Negotiable)

These constraints are “non-negotiable” in the sense that violating them may produce impressive demos but will undermine the research goal: defensible, inspectable internal change over time.

1. Architecture-first over results-first  
   Prefer stable loops and interfaces over benchmark chasing.

2. Persistent, inspectable internal signals  
   Internal signals must survive a run so they can be audited, replayed, and compared.

3. Explanations are coupled to meaningful internal change  
   Explanations must refer to internal deltas and triggers, not only output narratives.

4. Self-modification is bounded and safety-checked  
   Adaptation is allowed, but it must be rate-limited, constrained, and reviewable.

5. Separation of concerns preserves interpretability  
   Triggers, metrics, explanations, and revisions remain distinct so analysis stays causal.

6. Traceability and reproducibility over novelty  
   Prefer runs that can be reconstructed over “cleverness” that cannot be audited.

7. Failure is data  
   Failures are first-class artifacts that inform design and evaluation.

8. Status is declared plainly  
   NeuroForge is a foundational research prototype — not a product.

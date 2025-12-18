# Why Not “Just An LLM”?

This document controls scope. It explains why NeuroForge should not be compared directly to LLM-centric systems, without claiming that LLMs are “bad.”

## The Category Error

LLMs are primarily optimized as sequence models. NeuroForge is organized as a cognitive architecture: interacting loops that persist state, measure internal signals, explain changes, and apply bounded adaptation.

## What NeuroForge Emphasizes

- Long-lived internal state with persistence across runs
- Internal metrics (e.g., trust/coherence/error) treated as first-class signals
- Structured explanations for internal changes
- Safety-bounded self-modification

## What This Implies For Comparisons

If you compare NeuroForge to an LLM, compare along:
- auditability of internal state transitions
- reproducibility of adaptations across sessions
- quality and persistence of explanation artifacts
- safety constraints around self-modification

Not along:
- chatbot fluency
- benchmark scores designed for static models


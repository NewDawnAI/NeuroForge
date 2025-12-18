# How To Read This Repo

This file provides reading paths by audience.

NeuroForge is a cognitive architecture research system (not a library/product). The docs are organized into tiers so different readers can stop at the layer that matches what they need.

## The Map (In One Minute)

Start in this folder, then branch:
- `README.md` — what NeuroForge is (and is not)
- `CORE_PRINCIPLES.md` — constraints that govern all design decisions
- `WHY_NOT_LLM.md` — scope control; what comparisons do and do not mean here
- `GLOSSARY.md` — stable vocabulary for “internal signals”, “revision”, “auditability”, etc.

Then use tier indexes as navigation maps:
- `../01_FOUNDATION/INDEX.md` — conceptual foundation (“why this approach?”)
- `../02_ARCHITECTURE/INDEX.md` — system blueprint (“what exists?”)
- `../03_MECHANISMS/INDEX.md` — mechanisms (“how it changes?”)
- `../04_EXPERIMENTS/INDEX.md` — evidence (“what was tested?”)
- `../05_STATUS_AND_ROADMAP/INDEX.md` — current reality and next steps

## Reading Paths

### Path 1: New Serious Reader (30–60 minutes)
1. `README.md`
2. `CORE_PRINCIPLES.md`
3. `WHY_NOT_LLM.md`
4. `GLOSSARY.md`
5. `../01_FOUNDATION/INDEX.md`

### Path 2: Researcher / Reviewer (60–120 minutes)
1. `README.md`
2. `../01_FOUNDATION/INDEX.md`
3. `../02_ARCHITECTURE/INDEX.md`
4. `../04_EXPERIMENTS/INDEX.md`
5. `../05_STATUS_AND_ROADMAP/INDEX.md`

### Path 3: Engineer / Builder (30–90 minutes)
1. `../02_ARCHITECTURE/INDEX.md`
2. `../03_MECHANISMS/INDEX.md`
3. `../05_STATUS_AND_ROADMAP/INDEX.md`

### Path 4: Evidence-First (30–60 minutes)
1. `../04_EXPERIMENTS/INDEX.md`
2. `../05_STATUS_AND_ROADMAP/INDEX.md`

## What To Ignore Initially

Until you have a stable mental model from `README.md` + `CORE_PRINCIPLES.md`, avoid deep-diving into `03_MECHANISMS` or experiment artifacts. Those tiers are dense by design and can distort first impressions if read without context.

1. `../04_EXPERIMENTS/INDEX.md`
2. `../05_STATUS_AND_ROADMAP/INDEX.md`

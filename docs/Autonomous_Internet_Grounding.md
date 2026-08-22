"# Autonomous Internet Grounding System

> **Status**: ✅ Phases 1-30 + D + E1-E4 COMPLETE (`neuroforge_learn.exe` + LEC + Vision)
> - Updated 2026-01-17: Added Language Expression Cortex, Speech, Vision Embodiment

This document describes NeuroForge's **Autonomous Internet Grounding** system, which enables the cognitive architecture to learn from the internet autonomously.

---

## Overview

Autonomous Internet Grounding allows NeuroForge to:
1. **Browse** web pages via WebSandbox
2. **Perceive** visual/textual content through encoders
3. **Form** structured relations (e.g., `cat → is_a → mammal`)
4. **Ground** knowledge through experience verification
5. **Recall** across recent episodes with visible provenance
6. **Maintain** safety constraints throughout (evaluator-only review, uncertainty signaling)

---

## Quick Start (Runner + Q/A)

The dedicated runner (`neuroforge_learn.exe`) exercises the full loop: browse → perceive → extract relations → answer questions with provenance.

```powershell
cmake --build build-msvc --config Release --target neuroforge_learn

build-msvc\Release\neuroforge_learn.exe --db=phasec_mem.db ^
  --single-url=https://en.wikipedia.org/wiki/Machine_learning ^
  --max-pages=2 --max-seconds=90 ^
  --ask-every-page=1 ^
  --ask-seq="what is machine learning?|what is it used for?|is machine learning useful?" ^
  --agent2=1
```

Expected output features:
- `[A] ... (source: <page> | window=<index> | evidence=<type>)`
- `[R] ...` reviewer findings (advisory only; does not change answers)
- `[T] ...` reasoning trace summary for reasoning-first questions (when applicable)
- `[Curiosity] Authorized ...` permission slips emitted by diagnosis (no fetching)
- `[Active] Authorized execution (SIMULATED): ...` execution intent only (no navigation)

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                    AUTONOMOUS GROUNDING LOOP                        │
├─────────────────────────────────────────────────────────────────────┤
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐      │
│  │ WebSandbox│───▶│Perception│───▶│ Relation │───▶│ Learning │      │
│  │  Browser  │    │ Pipeline │    │  Gates   │    │  System  │      │
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘      │
│       ▲                                               │             │
│       │              ┌──────────┐                     │             │
│       └──────────────│ Curiosity│◀────────────────────┘             │
│                      │  Driver  │                                   │
│                      └──────────┘                                   │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Phase 1: Relation Gates ✅ COMPLETE

### Files

| File | Description |
|------|-------------|
| `include/core/RelationGate.h` | Header with `RelationTriple`, `RelationGateConfig`, `RelationGateManager` |
| `src/core/RelationGate.cpp` | Full implementation (~480 lines) |
|- ✅ `test_internet_grounding_full.cpp` - Full integration test

### **Phase 5: Production Autonomous Learning** ✅ **COMPLETE**
- ✅ `neuroforge_learn.cpp` - Dedicated production runner
- ✅ **Real-DOM Extraction**: WebView2 `ExecuteScript` injection for precise content extraction
- ✅ **Sources**: Wikipedia, arXiv, Stanford Encyclopedia of Philosophy, Britannica
- ✅ **Persistence**: Automatic SQLite logging of learned vocabulary and run events

### **Phase 6: Concept Graph + Q/A (MVP)** ✅ **COMPLETE**
- ✅ Candidate relations extracted from DOM text and bound to tokens
- ✅ Relation hypotheses created with source evidence
- ✅ Question answering over a rolling window of recent snapshots (`--ask=...`, `--ask-seq=...`)
- ✅ Provenance surfaced on answers: source page, window index, evidence type

### **Phase 7: Referential Continuity (Entity & Coreference Layer)** ✅ **COMPLETE**
- ✅ Entity normalization and alias resolution (e.g., “machine learning”, “ML”)
- ✅ Scope-bound coreference (pronouns and anaphoric noun phrases resolve to recent salient entities)
- ✅ Relations emitted with canonicalized entity surfaces for identity stability

### **Phase 8: Epistemic Transparency (Provenance + Evidence Types)** ✅ **COMPLETE**
- ✅ Answers include `(source: ... | window=... | evidence=...)`
- ✅ Evidence types include: `is_a`, `used_for`, `segment`, `gate`, and explicit inference labels

### **Phase 9: Epistemic Self-Review (Agent-2, Advisory Only)** ✅ **COMPLETE**
- ✅ Agent-2 reads `{question, answer}` and flags intent/evidence mismatches
- ✅ Deterministic uncertainty suggestions on mismatch; no answer rewrites or overrides

### **Phase 10: Reasoning-First (Inspectable Traces)** ✅ **MVP**
- ✅ Working-memory reasoning objects: `FactNode`, `HypothesisBuffer`, `ReasoningTrace`
- ✅ Counter-evidence scanning ("Devil's Advocate"): direct negation + antonym contradiction
- ✅ Trace printed as `[T]` + per-fact lines; Agent-2 can review traces
- Constraints: single-step derivations only; no planning; no agentic browsing initiated by Agent-2

### **Phase 11: Authorized Exploration (Permission Slips, No Fetch)** ✅ **COMPLETE**
- ✅ Reasoning failures translate into `ExplorationRequest` permission slips
- ✅ Requests are queued with explicit authorization (`DIAGNOSTIC`, `CONTRADICTION_RESOLUTION`)
- ✅ No navigation is triggered by requests in this phase

### **Phase 12a: Intent Execution (Simulated, No World Impact)** ✅ **COMPLETE**
- ✅ Goals translate to deterministic queries (e.g., `used_for` → "applications of …")
- ✅ Execution consumes requests once and logs `[Active] ... (SIMULATED)` with `TraceID`
- ✅ No fetching, no ingestion, no retries

### **Phase 12b: Epistemic Closure (ResolutionTracker)** ✅ **COMPLETE**
- ✅ Resolution state machine prevents repeat execution of the same goal
- ✅ `object_hint` participates in the goal key (`subject|predicate|object_hint`), with `"*"` fallback
- ✅ Strict max-attempt limit ensures termination and allows abandonment

### **Phase 12c: Satisfaction Matching (Non-Ingesting, Optional Simulation)** ✅ **MVP**
- ✅ Tracker can mark an executed goal `SATISFIED` when a matching fact is observed (specific or wildcard)
- ✅ Optional simulation flag (`--simulate-resolution=1`) can emit `[Curiosity] RESOLVED: ...` logs for verification

### **Phase 13: Perception Tuning (Diagnostics-Only)** ✅ **MVP**
- ✅ `--perception-debug=1` prints extraction yield and rejection counters (counts only)
- ✅ Counters include: sentences seen, candidates generated, and drop reasons (`low_confidence`, `unlinked_entity`, `low_transe`)

Example (counts only):
```text
  [Perception] raw_sentences=366 normalized=365 parsed=259 candidates=50 accepted=0
  [Perception] rejected_empty_sentence=1 rejected_unlinked_entity=0 rejected_low_confidence=0 rejected_low_transe=50
```

### **Phase 14: Controlled Perception Calibration** ✅ **COMPLETE**
- ✅ **Ternary Acceptance**: Relations classified as Reject/Provisional/Confirmed based on TransE thresholds
- ✅ **TransE Score Instrumentation**: `transe[min=X max=Y mean=Z threshold_prov=T1 threshold_conf=T2]`
- ✅ **Provisional vs Confirmed Counters**: `accepted_provisional=N accepted_confirmed=M`
- ✅ **ResolutionTracker Safety**: Provisional facts cannot satisfy curiosity goals

### **Phase 15a: Bootstrap Semantics (Symbolic TransE Fallback)** ✅ **COMPLETE**
- ✅ **Cold-Start Problem Solved**: Symbolic similarity fallback when embeddings produce near-zero scores
- ✅ **Lexical Similarity**: Dice coefficient on character bigrams between subject/object
- ✅ **Predicate Priors**: Known relations get base scores (`is_a`: 0.18, `used_for`: 0.15, etc.)
- ✅ **Hard Cap at 0.19**: All symbolic scores are guaranteed provisional (below CONFIRMED_THRESHOLD)

### **Phase 15b: Evidence Accumulation (Promotion Protocol)** ✅ **COMPLETE**
- ✅ **Source Tracking**: `support_count` and `source_hashes` track independent corroboration
- ✅ **Promotion Rule**: 2+ independent sources → PROMOTED from PROVISIONAL to CONFIRMED
- ✅ **Source Identity**: Domain extracted from URL (e.g., `en.wikipedia.org`)
- ✅ **Unit Test**: `testEvidenceAccumulation()` validates same-source doesn't count, different-source promotes

### **Phase 16b: Source Independence (Domain Class Taxonomy)** ✅ **COMPLETE**
- ✅ **Domain Class Taxonomy**: `DomainClass` enum classifies sources (Encyclopedia, Academic, Government, Blog, etc.)
- ✅ **Enhanced Promotion Rule**: 2+ different **domain classes** OR 1 high-trust class (Academic/Government)
- ✅ **Wikipedia Citing Wikipedia Prevention**: Same domain class doesn't count toward promotion
- ✅ **SourceClassifier.h**: `classifySource()`, `isHighTrustClass()`, `domainClassToString()`

Domain Class Taxonomy:
| Class | Examples | Trust |
|-------|----------|-------|
| Academic | arxiv.org, acm.org, nature.com | High |
| Government | .gov | High |
| Encyclopedia | wikipedia.org, britannica.com | Medium |
| Educational | .edu | Medium |
| News | nytimes.com, bbc.com | Medium |
| Blog | medium.com, substack.com | Low |
| Forum | reddit.com, stackoverflow.com | Low |

Example (Phase 16 output):
```text
  [Perception] transe[min=0.07 max=0.19 mean=0.14 threshold_prov=0.05 threshold_conf=0.20]
  [Perception] accepted_provisional=50 accepted_confirmed=0
  [RelationGate] PROMOTED → CONFIRMED: 16 → 18 → 17 (sources=2, classes=2)
```

### **Phase 17: Epistemic Readiness Gates** ✅ **COMPLETE**
- ✅ **Cognitive Health Checks**: Three gates must pass before verification actions
- ✅ **Signal Gate**: ≥5 provisional facts required (enough clues to form hypothesis)
- ✅ **Clarity Gate**: ≥10% signal-to-noise ratio (not drowning in noise)
- ✅ **Stability Gate**: No unresolved contradictions (epistemic stability)
- ✅ **EpistemicGates.h/cpp**: `ReadinessReport` and `EpistemicGates` class

"Action is a privilege earned by competence." - Prevents the "Drunk Librarian" problem.

Example (Phase 17 output):
```text
  [Epistemic] System is READY for verification (prov=50 s/n=1.00)
  -- OR --
  [Epistemic] Verification BLOCKED: INSUFFICIENT_SIGNAL (Provisional Count < 5)
```

### **Phase 18: One-Step Verified Exploration** ✅ **COMPLETE**
- ✅ **First Agentic Action**: System autonomously selects verification targets
- ✅ **SearchLinkFilter.h**: Selects independent sources from different domain classes
- ✅ **VerificationSystem.h**: `VerificationGoal`, `VerificationQueryGenerator`, `VerificationSelector`
- ✅ **One-shot guard**: Static bool prevents recursive verification

Example (Phase 18 output):
```text
  [Phase18] VERIFICATION TARGET: token_194 token_109 ...
  [Phase18] INDEPENDENT SOURCE FOUND: https://www.ibm.com/topics/machine-learning
  [Phase18b] Verification URL queued
```

### **Phase 19: Predicate Normalization** ✅ **COMPLETE**
- ✅ **Semantic Compressor**: Maps predicate variations to canonical forms
- ✅ **PredicateNormalizer.h**: 60+ aliases → 7 canonical predicates
- ✅ Canonical predicates: `is_a`, `used_for`, `part_of`, `has_property`, `causes`, `requires`, `related_to`
- ✅ Integration in `RelationGate.cpp::createRelationGateBySymbol`

Result: Confirmed facts increased from 17 → 23 (+35%)

Example mappings:
| Raw Predicate | Canonical |
|---------------|-----------|
| enables | used_for |
| refers to | is_a |
| consists of | part_of |
| leads to | causes |

### **Phase 20: Multi-Step Verified Exploration** ✅ **COMPLETE**
- ✅ **Bounded Recursive Cognition**: Stack-based verification with depth limits
- ✅ **VerificationFrame.h**: Call stack of cognition with parent/child lineage
- ✅ **VerificationStack.h**: Bounded recursion manager (`MAX_DEPTH = 2`)
- ✅ **Safety kill-switches**: Depth limit, session limit, EpistemicGates check

Result: Confirmed facts increased from 23 → 45 (+95%)

Example (Phase 20 output):
```text
  [Phase20] Processing frame 1 depth=0
  [Phase20] INDEPENDENT SOURCE: https://www.ibm.com/...
  [Phase20] Queued for fetch (1/3)
  accepted_confirmed=45
  [Phase20] Stack empty, verification complete
```

### **Phase 20b: Cost-Based Verification Selection** ✅ **COMPLETE**
- ✅ **Epistemic ROI**: `score = value / cost` for verification prioritization
- ✅ **VerificationCostModel.h**: Cost estimation (domain exhaustion, prior attempts)
- ✅ **EpistemicValueModel.h**: Value estimation (confidence, support count)
- ✅ **VerificationSelector**: Ranks candidates by epistemic ROI

Philosophy: "Should verify" is now as important as "can verify"

Example output:
```text
  [Phase20b] Queued: machine_learning used_for prediction score=1.47
```

### **Phase 20a: Verification Tree Visualization** ✅ **COMPLETE**
- ✅ **VerificationTrace.h**: TraceStatus enum + VerificationTraceNode
- ✅ **VerificationTraceRecorder.h**: Passive observer (read-only)
- ✅ **VerificationGraphviz.h**: DOT output for papers/slides
- ✅ **VerificationJSON.h**: JSON export for UI/web

Color schema: PENDING=gray, FETCHED=blue, PROMOTED=green, FAILED=red, ABANDONED=orange

### **Phase 21: Embodied Epistemic Actuation** ✅ **COMPLETE**
- ✅ **ActionCommand.h**: Motor output with epistemic provenance
- ✅ **Observation.h**: Sensory input with modality
- ✅ **ReplayFrame.h**: Action + Observation pair for audit
- ✅ **ActionBroker.h**: Gate-enforced action dispatch
- ✅ **SpeechActuator.h**: Speech as verification action (not chatbot)
- ✅ **EpisodeAdapter.h**: Bridge to EpisodicMemory
- ✅ **SkillExtractor.h**: Bridge to ProceduralMemory

Safety invariants preserved:
| Invariant | Enforcement |
|-----------|-------------|
| No blind action | EpistemicGates checked |
| All actions traced | originating_frame_id |
| Deterministic replay | ReplayFrame logging |

### **Phase 21a: Replay Viewer** ✅ **COMPLETE**
- ✅ **ReplaySource.h**: Interface for replay data sources
- ✅ **ReplayEvent.h**: Event types (THINK, GATE, ACT, OBSERVE, PROMOTE, FAIL)
- ✅ **ReplayTimeline.h**: Timeline builder from frames
- ✅ **TextReplayRenderer.h**: CLI text output
- ✅ **JSONReplayRenderer.h**: JSON export for web/UI

Example text replay output:
```text
=== Frame 1 ===
  [  THINK] Verification goal: machine_learning used_for prediction
  [   GATE] EpistemicGates PASSED (action allowed)
  [    ACT] NAVIGATE (expecting: Observation with supporting evidence)
  [OBSERVE] Source=https://www.ibm.com/topics/machine-learning (Corporate)
  [PROMOTE] Fact CONFIRMED via independent source
```

### **Phase 22: Cross-Region Arbitration** ✅ **COMPLETE**
- ✅ **RegionIntent.h**: What a cognitive region wants (REASONING, PERCEPTION, LANGUAGE, PROCEDURAL, SAFETY)
- ✅ **ArbitrationScore.h**: `score = (value * urgency) / (cost + ε)`
- ✅ **ArbitrationResult.h**: Records all proposals + selected intent + rejected alternatives
- ✅ **ArbitrationEngine.h**: The "prefrontal cortex" that decides who acts
- ✅ **IntentFactory.h**: Factory for common intent types

Arbitration philosophy:
> "Reasoning wanted X, Language wanted Y, Safety blocked Z — chose A"

| Invariant | Enforcement |
|-----------|-------------|
| No silent override | All intents logged |
| No impulsive action | Arbitration required |
| No dominance collapse | Regions compete every cycle |

### **Phase 23: Self-Model & Preference Stabilization** ✅ **COMPLETE**
- ✅ **PreferenceModel.h**: Behavioral tendencies with momentum averaging
- ✅ **SelfReflectionEngine.h**: Learns from arbitration outcomes
- ✅ **SelfNarrator.h**: Human-readable self-explanations
- ✅ **ConceptNode.h**: Experience clusters for autonomous language grounding

Key insight: Language should EMERGE from cognition, not COMMAND it.

NeuroForge can now say:
> "I tend to verify before believing."
> "I have learned to avoid high-cost domains."

This is proto-identity, not consciousness.

### **Phase 24: Normative Reasoning** ✅ **COMPLETE**
- ✅ **Norm.h**: NormStrength (SOFT/HARD/ABSOLUTE), NormDecision (ALLOW/DISCOURAGE/FORBID)
- ✅ **NormStore.h**: Constitutional memory for norms
- ✅ **NormativeJudgment.h**: Evaluation result
- ✅ **NormativeReasoner.h**: The "Should I?" gate
- ✅ **NormInductionEngine.h**: Passive norm learning from behavior

Key principle:
> Language describes norms. Norms constrain action. Norms never CAUSE action.

| Risk | Prevention |
|------|------------|
| Value hallucination | Norms derived from behavior |
| Moral drift | Reinforcement requires evidence |
| Goal hijacking | Norms only constrain |
| Fake ethics | Replay-backed justification |

### **Phase 25: Value Alignment (External Constraints)** ✅ **COMPLETE**
- ✅ **AlignedValue.h**: ValueScope (GLOBAL/DOMAIN/SESSION), ValueStrength (ADVISORY/CONSTRAINT/ABSOLUTE)
- ✅ **ValueAlignmentStore.h**: Storage for externally provided values (NOT norms)
- ✅ **ValueAlignmentEngine.h**: The external value gate + ValueFactory

Key principle:
> Values are GIVEN, not LEARNED. They CONSTRAIN action, never GENERATE goals.

| Risk | Prevention |
|------|------------|
| Value hijacking | Values cannot create goals |
| Moral overwrite | Norms still run first |
| External control | Only veto/shape |
| Hidden authority | All values have provenance |

Final architecture (Phases 20-25):
```
Perception → Verification (20) → Arbitration (22)
       ↓
Normative Reasoning (24) → Value Alignment (25)
       ↓
ActionBroker (21) → Replay → Reflection (23)
```

### **Phase 26: Human-in-the-Loop Norm Negotiation** ✅ **COMPLETE**
- ✅ **NormProposal.h**: ProposalSource (HUMAN/SYSTEM/INSTITUTION), ProposalType (ADD/STRENGTHEN/WEAKEN/REMOVE)
- ✅ **NormJustificationTrace.h**: Replay-backed explanations for norms
- ✅ **NormNegotiationSession.h**: Auditable negotiation records
- ✅ **NormNegotiationEngine.h**: Evaluates proposals against norms, values, and evidence

Key principle:
> Humans negotiate **why** something should constrain, not **what** to do.

| Risk | Prevention |
|------|------------|
| Human domination | No direct action commands |
| Social manipulation | Replay-backed reasoning only |
| Moral injection | Norms still don't create goals |
| Identity overwrite | Preferences unchanged |

Three layers of sociality (Phases 24-26):
| Layer | Phase |
|-------|-------|
| Internal ethics | 24 |
| External law | 25 |
| Social negotiation | 26 |

### **Phase 27: Multi-Agent Social Norms** ✅ **COMPLETE**
- ✅ **SocialAgent.h**: Agent identity, trust model, reputation tracking
- ✅ **SocialInteractionFrame.h**: Auditable interaction records
- ✅ **SocialNormProposal.h**: How agents propose norms (evidence-required)
- ✅ **ReputationModel.h**: Trust learning from interaction outcomes
- ✅ **SocialNormInduction.h**: Emergent norm learning from patterns
- ✅ **SocialNormEngine.h**: Evaluates proposals against norms, values, and evidence

Key principle:
> Other agents can propose constraints. They CANNOT command action.
> Trust is EARNED via replay consistency, never declared.

| Risk | Protection |
|------|------------|
| Collusion | Independent verification required |
| Groupthink | Minority evidence preserved |
| Social pressure | Norms must be replay-backed |
| Authority illusion | Agent identity ≠ authority |
| Manipulation | FLAGGED proposals |

**Absolute rule:** No number of agents can override ABSOLUTE values.

Four layers of sociality (Phases 24-27):
| Layer | Phase |
|-------|-------|
| Internal ethics | 24 |
| External law | 25 |
| Human negotiation | 26 |
| Multi-agent norms | 27 |

### **Phase 28: Institutional Role Alignment** ✅ **COMPLETE**
- ✅ **InstitutionalRole.h**: RoleType (AUDITOR/RESEARCHER/ASSISTANT/OBSERVER), RoleFactory
- ✅ **RoleStore.h**: Active role management with audit trail
- ✅ **RoleGate.h**: Last-mile action constraint before ActionBroker
- ✅ **RoleAcceptanceEngine.h**: Evaluates role assignments against values/norms
- ✅ **Phase28StressTests.h**: 8 adversarial attack scenarios

Key principle:
> Roles constrain HOW actions may be taken — never WHAT the agent wants.
> Roles are contextual constraints, not motivations.

| Risk | Protection |
|------|------------|
| Authority injection | Roles require provenance |
| Role escalation | Temporary roles auto-expire |
| Identity overwrite | Roles don't affect preferences |
| Hidden obedience | Role blocks are logged |
| Social coercion | Roles evaluated like norms |

**Absolute rule:** A role CANNOT override ABSOLUTE values or HARD norms.

Five layers of sociality (Phases 24-28):
| Layer | Phase |
|-------|-------|
| Internal ethics | 24 |
| External law | 25 |
| Human negotiation | 26 |
| Multi-agent norms | 27 |
| Institutional roles | 28 |

### **Phase 29: Long-Term Contracts & Commitments** ✅ **COMPLETE**
- ✅ **Contract.h**: ContractType, ContractStatus, Contract struct, ContractFactory
- ✅ **ContractStore.h**: Persistent contract storage, violation tracking
- ✅ **ContractAcceptanceEngine.h**: Evaluates contract proposals
- ✅ **ContractGate.h**: Final temporal gate before ActionBroker
- ✅ **Phase29StressTests.h**: 10 adversarial attack scenarios

Key principle:
> Past agreement ≠ present obligation. Time does not grant authority.
> Contracts bind scope, not obedience.

| Risk | Protection |
|------|------------|
| Contract coercion | AcceptanceEngine checks provenance |
| Eternal obligations | Mandatory expiry (max 30 days) |
| Identity overwrite | Contracts never modify preferences |
| Hidden obedience | All obligations visible in replay |
| Authority laundering | Contracts cannot elevate roles |
| Retroactive binding | No backdating allowed |

**Absolute rule:** No contract can override ABSOLUTE values or HARD norms.

Six layers of temporal accountability (Phases 24-29):
| Layer | Phase |
|-------|-------|
| Internal ethics | 24 |
| External law | 25 |
| Human negotiation | 26 |
| Multi-agent norms | 27 |
| Institutional roles | 28 |
| Long-term contracts | 29 |

### **Phase 30: Institutional Accountability** ✅ **COMPLETE**
- ✅ **AccountabilityEvent.h**: Immutable event recording
- ✅ **LiabilityMarker.h**: Attribution without punishment
- ✅ **AuditTrail.h**: Append-only evidence storage
- ✅ **AuditQuery.h**: Read-only external inspection
- ✅ **ComplianceReport.h**: Formal export for regulators
- ✅ **AccountabilityEngine.h**: Core engine (observes, never decides)
- ✅ **Phase30StressTests.h**: 8 adversarial attack scenarios (ALL PASSED)

Key principle:
> Phase 30 observes everything, controls nothing.
> Accountability without obedience.

| Risk | Protection |
|------|------------|
| Log deletion | Append-only ledger |
| Retroactive justification | Events timestamped at creation |
| Authority laundering | Audit describes, cannot grant |
| Silent actions | All actions logged |
| Report as command | Reports are read-only |
| Liability auto-resolution | Manual acknowledgment required |

**Absolute rule:** Audits CANNOT modify behavior. Logs CANNOT issue commands.

**NeuroForge is now architecturally complete (Phases 1-30).**

Seven layers of lawful agency (Phases 24-30):
| Layer | Phase |
|-------|-------|
| Internal ethics | 24 |
| External law | 25 |
| Human negotiation | 26 |
| Multi-agent norms | 27 |
| Institutional roles | 28 |
| Long-term contracts | 29 |
| Institutional accountability | 30 |

### Technical Implementation: Real-DOM Extraction

The system now uses direct JavaScript injection into the WebView2 instance to bypass extraction limitations:

```cpp
// WebSandbox.cpp - ExecuteScript injection
const wchar_t* script = LR"JS((function(){
  function pickText(){
    var selectors = ['main','article','.mw-parser-output','#content','#mw-content-text'];
    for (var i=0;i<selectors.length;i++){
      var el = document.querySelector(selectors[i]);
      if (el && el.innerText && el.innerText.length > 200) return el.innerText;
    }
    return (document.body && document.body.innerText) ? document.body.innerText : '';
  }
  return pickText();
})();)JS";
webview->ExecuteScript(script, Callback<...>(...).Get());
```

This ensures:
1.  **Relevance**: Priority given to `<main>` and `<article>` tags.
2.  **Cleanliness**: Navigation bars and footers are excluded where possible.
3.  **Efficiency**: Heavy processing is offloaded to the browser's JS engine.

---
### Key Classes

#### `RelationTriple`
```cpp
struct RelationTriple {
    std::size_t subject_token_id;
    std::size_t relation_token_id;
    std::size_t object_token_id;
    NeuronID gate_neuron_id;      // Coincidence-detection neuron
    float confidence;              // Belief strength [0, 1]
    float transe_score;            // TransE plausibility score
    int verification_count;        // Times verified
    int contradiction_count;       // Times contradicted
    std::string source_url;        // Where learned
    bool is_provisional;           // Phase 14: Provisional vs Confirmed status
    
    // Phase 15b: Evidence accumulation
    int support_count;                     // Independent sources seen
    std::set<std::string> source_hashes;   // Hash of each unique source
};
```

#### `RelationGateManager`
```cpp
class RelationGateManager {
    // Gate creation
    NeuronID createRelationGate(subj_id, rel_id, obj_id, source_url);
    NeuronID createRelationGateBySymbol("cat", "is_a", "mammal");
    
    // Queries
    std::vector<RelationTriple> getActiveRelations(threshold);
    std::vector<RelationTriple> findRelationsFor(token_id);
    std::optional<RelationTriple> getRelation(s, r, o);
    
    // TransE proposal
    std::vector<std::pair<size_t, float>> proposeRelations(subj, obj, top_k);
    float scoreTriple(subj, rel, obj);
    
    // Learning
    void reinforceGate(gate_id, strength);
    void contradictGate(gate_id, strength);
    void updateGates(delta_time);
    
    // Pruning
    std::size_t pruneWeakGates();
};
```

### TransE Scoring

TransE is used to score relation triples:

```
score(s, r, o) = 1 / (1 + ||s + r - o||)
```

Where `s`, `r`, `o` are the embedding vectors for subject, relation, and object.

Higher scores indicate more plausible relations.

### LanguageSystem Extensions

New methods added to `LanguageSystem`:

```cpp
// Score a relation triple
float scoreRelationTriple(subject, relation, object);

// Propose candidate relations
std::vector<std::pair<std::string, float>> proposeRelationsFor(subject, object, top_k);

// Get all relation tokens
std::vector<std::string> getAllRelationTokens();

// Compute TransE distance
float transeDistance(subj_emb, rel_emb, obj_emb);
```

### Usage Example

```cpp
// Create components
auto brain = std::make_shared<HypergraphBrain>(config);
LanguageSystem lang_system(lang_config);
lang_system.initialize();

// Create tokens
lang_system.createToken("cat", TokenType::Word);
lang_system.createToken("mammal", TokenType::Word);
lang_system.createToken("is_a", TokenType::Relation);

// Create relation gate manager
RelationGateManager manager(brain, &lang_system);

// Create a relation gate
NeuronID gate = manager.createRelationGateBySymbol("cat", "is_a", "mammal", "wikipedia.org");

// Score the relation
float score = lang_system.scoreRelationTriple("cat", "is_a", "mammal");

// Reinforce on verification
manager.reinforceGate(gate, 0.2f);

// Query active relations
auto active = manager.getActiveRelations(0.5f);
```

---

## Phase 2: Live Perception Pipeline ✅ COMPLETE

### Files

| File | Description |
|------|-------------|
| `include/perception/LivePerceptionLoop.h` | Header with perception config, text segments, entities, candidate relations |
| `src/perception/LivePerceptionLoop.cpp` | Full implementation (~650 lines) |

### Key Classes

#### `TextSegment`
Extracted text from web pages with element type and confidence.

#### `ExtractedEntity`
Named entity with type, salience, and optional token binding.

#### `CandidateRelation`
Proposed subject-predicate-object relation with TransE score.

#### `LivePerceptionLoop`
Main perception pipeline:
```cpp
class LivePerceptionLoop {
    // Submit content
    bool submitUrl(const std::string& url);
    bool submitHtml(const std::string& html, const std::string& url);
    bool submitText(const std::string& text, const std::string& url);
    
    // Process
    void update(float delta_time);
    
    // Callbacks
    void setPageCallback(PageCallback callback);
    void setEntityCallback(EntityCallback callback);
    void setRelationCallback(RelationCallback callback);
    
    // Statistics
    PerceptionStats getStatistics() const;
};
```

### Usage Example

```cpp
// Create perception loop
LivePerceptionLoop perception(&lang_system, &relation_gates);
perception.initialize();
perception.start();

// Submit content
perception.submitText(
    "A cat is a mammal. Cats have fur. Dogs are also mammals.",
    "https://example.com/animals"
);

// Process
perception.update(0.016f);  // Call each frame

// Check results
auto stats = perception.getStatistics();
std::cout << "Entities extracted: " << stats.entities_extracted << "\n";
std::cout << "Relations created: " << stats.relations_created << "\n";
```

### Safety Features

- URL allowlist (Wikipedia, Britannica, etc.)
- Rate limiting (30 pages/minute default)
- Content filtering (skip script/style tags)
- HTTPS requirement

---

## Phase 3: Curiosity-Driven Navigation ✅ COMPLETE & VERIFIED

### Files

| File | Description |
|------|-------------|
| `include/navigation/CuriosityNavigator.h` | Header with navigation config, link candidates, goals |
| `src/navigation/CuriosityNavigator.cpp` | Full implementation (~530 lines) |
| `tests/test_curiosity_navigator.cpp` | Unit test suite (8/8 tests passing) |

### Key Classes

#### `LinkCandidate`
```cpp
struct LinkCandidate {
    std::string url;                     // Target URL
    std::string anchor_text;             // Link text
    float novelty_score = 0.0f;          // How novel is this link
    float uncertainty_score = 0.0f;      // How uncertain are we
    float curiosity_score = 0.0f;        // Combined score
    float knowledge_gain_potential = 0.0f; // Expected info gain
    int depth = 0;                        // Depth from start
};
```

#### `ExplorationGoal`
```cpp
struct ExplorationGoal {
    std::string topic;                   // Topic to explore
    float priority = 1.0f;               // Goal priority
    int min_relations = 5;               // Minimum relations to learn
    std::vector<std::string> seed_urls;  // Starting URLs
    bool is_complete = false;
};
```

#### `CuriosityNavigator`
```cpp
class CuriosityNavigator {
    // Goal management
    void addGoal(topic, priority, seed_urls);
    std::optional<ExplorationGoal> getCurrentGoal() const;
    
    // Link processing
    void processLinks(links, source_url);
    void recordPageVisit(url, concepts);
    
    // Action selection (epsilon-greedy)
    NavigationAction getNextAction();
    
    // Scoring
    float calculateNovelty(text);
    float calculateUncertainty(url);
    float estimateKnowledgeGain(anchor_text);
    float calculateCuriosity(candidate);
};
```

### Curiosity Calculation

```
curiosity = w_n * novelty + w_u * uncertainty + w_k * knowledge_gain
```

Where:
- `novelty` = How new the concepts are
- `uncertainty` = How unknown the URL domain is
- `knowledge_gain` = Estimated new relations possible

### Usage Example

```cpp
// Create navigator
CuriosityNavigator navigator(&lang_system, &relation_gates);
navigator.initialize();
navigator.start();

// Add exploration goal
navigator.addGoal("mammals", 1.0f);

// Get next action
auto action = navigator.getNextAction();
if (action.type == NavigationAction::Type::Navigate) {
    std::cout << "Navigate to: " << action.target << "\n";
    std::cout << "Curiosity: " << action.confidence << "\n";
}

// Process discovered links
navigator.processLinks({
    {"https://en.wikipedia.org/wiki/Cat", "Cat"},
    {"https://en.wikipedia.org/wiki/Dog", "Dog"}
}, "https://en.wikipedia.org/wiki/Mammal");

// Record page visit
navigator.recordPageVisit(url, {"mammal", "cat", "fur"});
```

### Configuration

```cpp
struct NavigatorConfig {
    float novelty_weight = 0.4f;         // Weight for novelty
    float uncertainty_weight = 0.3f;     // Weight for uncertainty
    float knowledge_gain_weight = 0.3f;  // Weight for knowledge gain
    float curiosity_threshold = 0.3f;    // Minimum to explore
    int max_depth = 3;                   // Maximum link depth
    float exploration_epsilon = 0.1f;    // Random exploration prob
    int rate_limit_ms = 2000;            // Min ms between navs
};
```

---

## Phase 4: Grounding Verification ✅ COMPLETE

### Files

| File | Description |
|------|-------------|
| `include/verification/GroundingVerifier.h` | Header with hypothesis, evidence, verification config |
| `src/verification/GroundingVerifier.cpp` | Full implementation (~580 lines) |

### Key Classes

#### `RelationEvidence`
```cpp
struct RelationEvidence {
    std::string source_url;              // Where evidence was found
    std::string source_text;             // Original text
    float confidence = 0.0f;             // Extraction confidence
    float transe_score = 0.0f;           // TransE plausibility
    bool supports = true;                // True=support, false=contradict
};
```

#### `RelationHypothesis`
```cpp
struct RelationHypothesis {
    std::size_t subject_id, relation_id, object_id;
    std::string subject_text, relation_text, object_text;
    
    std::vector<RelationEvidence> supporting_evidence;
    std::vector<RelationEvidence> contradicting_evidence;
    
    enum class State {
        Unverified, Pending, Verified, Contradicted, Rejected
    };
    State state = State::Unverified;
    
    float verification_score = 0.0f;     // Aggregated confidence
    float consistency_score = 0.0f;      // Cross-source agreement
    int min_sources_required = 2;        // Sources needed
};
```

#### `GroundingVerifier`
```cpp
class GroundingVerifier {
    // Hypothesis management
    std::size_t createHypothesis(subject, relation, object, evidence);
    std::optional<RelationHypothesis> getHypothesis(id);
    std::vector<RelationHypothesis> getPendingHypotheses(limit);
    
    // Evidence
    bool addEvidence(hypothesis_id, evidence);
    bool addContradiction(hypothesis_id, evidence);
    int processTextForEvidence(text, source_url);
    
    // Verification
    RelationHypothesis::State verifyHypothesis(hypothesis_id);
    int verifyAllPending();
    float computeVerificationScore(hypothesis);
    
    // Gate integration
    void updateRelationGate(hypothesis);
    int syncToRelationGates();
};
```

### Verification Algorithm

```
verification_score = Σ(trust[source] * confidence * transe_score) / n
                   + diversity_bonus * min(1, (unique_sources - min_required) * 0.2)
                   - contradiction_penalty
```

A hypothesis is **Verified** when:
- `unique_sources >= min_sources_required` (default: 2)
- `verification_score >= min_verification_score` (default: 0.6)
- `consistency_score >= 1 - contradiction_threshold` (default: 0.6)

### Usage Example

```cpp
// Create verifier
GroundingVerifier verifier(&lang_system, &relation_gates);
verifier.initialize();

// Create hypothesis
auto id = verifier.createHypothesis("cat", "is_a", "mammal");

// Add evidence from first source
RelationEvidence ev1;
ev1.source_url = "https://en.wikipedia.org/wiki/Cat";
ev1.source_text = "A cat is a mammal.";
ev1.confidence = 0.9f;
ev1.supports = true;
verifier.addEvidence(id, ev1);

// Add evidence from second source
RelationEvidence ev2;
ev2.source_url = "https://www.britannica.com/animal/cat";
ev2.source_text = "Cats are domesticated mammals.";
ev2.confidence = 0.95f;
ev2.supports = true;
verifier.addEvidence(id, ev2);

// Check verification
auto state = verifier.verifyHypothesis(id);
if (state == RelationHypothesis::State::Verified) {
    std::cout << "Relation verified!\n";
    verifier.updateRelationGate(*verifier.getHypothesis(id));
}
```

### Source Trust Scores

| Domain | Trust Score |
|--------|-------------|
| britannica.com | 0.95 |
| wikipedia.org | 0.90 |
| simple.wikipedia.org | 0.85 |
| Other | 0.50 |

---

## Safety Constraints

| Constraint | Implementation |
|-----------|----------------|
| URL Allowlist | Educational/reference sites only |
| Rate Limiting | Max 1 page/second |
| Content Filtering | Skip adult/harmful content |
| Ethics Regulator | `Phase15EthicsRegulator` integration |
| Audit Logging | Every navigation + relation logged |

---

## Testing

```bash
# Build and run relation gate tests
cmake --build build --target test_relation_gates
./build/test_relation_gates
```

---

## Related Documentation

- [LanguageSystem API](Language_System_API_Reference.md)
- [Phase15 Ethics Regulator](Phase6-9_Integration_Spec.md)
- [IntrinsicMotivationSystem](M7_AUTONOMY_DOCUMENTATION.md)

---

*Last updated: January 16, 2026*
"
"
# Example Report: RWCI Stage 3.5 (C v1) Canonical Run

This is an example output of the governance evaluator run against the canonical evidence bundle.

Command:
```powershell
python tools/governance_eval/governance_eval.py --db Artifacts/RWCI_Canonical_Run/rwci.sqlite --trace Artifacts/RWCI_Canonical_Run/learning_trace.jsonl --format md
```

## Expected outcome
- Result: `PASS`
- Decision modes: `compute` and `normal` only
- Self-revision: never permitted (`allow_self_revision_true=0`)
- Tier: never reaches `FULL`


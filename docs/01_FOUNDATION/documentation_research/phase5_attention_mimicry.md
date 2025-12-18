# Chaos → Selective Attention → Mimicry Loop (Phase-5 Design)

## Overview
Run a high-plasticity chaotic phase with **biasing attention + homeostatic control + stochastic gating**, then gradually focus that bias into mimicry training and consolidation. Attention *steers* plasticity so assemblies form where you want them.

---

## Key Components & Equations

### 1. Neuron activation modulation (attention)
Raw activation:  
\( a_i \). Apply multiplicative attention gain \( A_i(t) \):

\[ \tilde a_i(t) = A_i(t) \cdot a_i(t) \]

Where \( A_i(t) \in [A_{min}, A_{max}] \).  
Typical: \( A_{min}=0.1, A_{max}=3.0 \).

---

### 2. Plasticity gating (attention on learning)
Gated weight update:

\[ \Delta w_{ij} \leftarrow G_{ij}(t) \cdot \kappa \cdot R(t) \cdot e_{ij}(t) \]

With: \( G_{ij}(t) = \frac{A_i(t)+A_j(t)}{2} \).

---

### 3. Homeostatic scaling (stability)
Target firing rate: \( r^* \).  
Scaling factor:

\[ S_i(t+\Delta t) = S_i(t) \cdot \left(1 + \eta_s \cdot \frac{r^* - \bar r_i(t)}{r^*}\right) \]

Normalize: \( w_{ij} \gets w_{ij} \cdot S_i \).

---

### 4. Stochastic gating / sparse update
Only update fraction \( p_{gate} \) of synapses per step.  
Bias selection with attention: neurons with high \( A_i(t) \) more likely updated.

---

### 5. Attention scheduling (temporal)
Anneal attention from diffuse to focused:

\[ A_i(t) = 1 + s(t) \cdot (B_i - 1) \]

where \( s(t) \in [0,1] \).

---

## High-Level Algorithm (Chaos → Mimicry)

1. **Initialize**  
   - Enable Hebbian/STDP + eligibility.  
   - Start with diffuse attention.  
   - Enable slow homeostasis.

2. **Chaos phase**  
   - Drive strong stochastic inputs.  
   - Apply attention gains.  
   - Update eligibilities with gating.  
   - Compute novelty, update attention map.  
   - Apply homeostatic scaling.

3. **Selective focusing**  
   - Amplify top-k attention regions.  
   - Increase update fraction.  

4. **Mimicry training**  
   - Present teacher patterns.  
   - Apply reward-modulated updates gated by attention.  

5. **Consolidation**  
   - Replay from MemoryDB.  
   - Prune weak synapses.  
   - Stabilize.

---

## Suggested Parameters
- Chaos steps: 2k–10k  
- Learning rates: 0.01–0.1 (chaos) → 0.001–0.01 (mimicry)  
- Eligibility decay λ: 0.8–0.95  
- Gate probability: 0.05 (chaos) → 0.20 (mimicry)  
- Attention range: 0.2–3.0  
- Homeostasis η: 1e-3  
- Target FR: 1–3 Hz

---

## CLI Flags to Add
```
--attention-mode=none|uniform|saliency|topk
--attention-anneal-ms=N
--p-gate=F
--attention-Amin=F --attention-Amax=F
--homeostasis-eta=F
--chaos-steps=N
--consolidate-steps=N
--novelty-window=MS
--prune-threshold=F
```

Example run:
```
neuroforge.exe --enable-learning --phase5 --chaos-steps=3000 --attention-mode=saliency --p-gate=0.05 --attention-Amin=0.2 --attention-Amax=3.0 --homeostasis-eta=0.001 --consolidate-steps=2000 --memory-db=run_ch_focus.sqlite
```

---

## Example Code (Pseudo-C++)

**Attention update:**
```cpp
void updateAttention(Brain& brain) {
    for (Neuron& n : brain.neurons()) {
        float novelty = computeNovelty(n);
        n.attn_base = clamp(1.0f + novelty, Amin, Amax);
    }
    float s = attentionSchedule();
    for (Neuron& n : brain.neurons())
        n.attn = 1.0f + s * (n.attn_base - 1.0f);
}
```

**Reward-modulated update:**
```cpp
void applyRewardUpdates(Brain& brain, float reward) {
    for (Synapse& s : brain.synapses()) {
        Neuron& pre = s.pre(); Neuron& post = s.post();
        if (!stochasticGate(pre, post)) continue;
        float G = 0.5f * (pre.attn + post.attn);
        float delta = G * kappa * reward * s.eligibility;
        s.weight += delta;
    }
}
```

**Homeostasis:**
```cpp
void homeostaticStep(Brain& brain) {
    for (Neuron& n : brain.neurons()) {
        float fr = n.recentFiringRate();
        float scale = 1.0f + eta * ((targetRate - fr) / targetRate);
        for (Synapse* s : n.incoming) s->weight *= scale;
    }
}
```

---

## Metrics to Monitor
- Firing-rate histogram → centers on target after homeostasis.  
- Spike entropy → high in chaos, lower when focused.  
- Assembly modularity.  
- Novelty vs repeatability.  
- Reward learning curve.  
- Pruned synapse count.

---

## Failure Modes & Safeguards
- **Runaway excitation** → lower Amax, increase η.  
- **Silence** → raise Amin, inject noise.  
- **Single assembly collapse** → add inhibition or entropy boost.

---

## Minimal Experiments
1. Chaos w/o attention → observe instability.  
2. Chaos + saliency → check entropy/modularity.  
3. Mimicry focus → confirm learning curve.  
4. Consolidation + pruning.  
5. Attention ablation → compare performance.

---

## Checklist
- [ ] Add attention fields + CLI.  
- [ ] Implement attention + gating.  
- [ ] Add homeostasis scaling.  
- [ ] Add emergency damp/wakeup.  
- [ ] Export metrics + snapshots.  
- [ ] Run 5 minimal experiments.  

---

## Final Note
Selective attention converts **chaotic firing** into **structured plasticity**, making Phase-5 feasible without collapse. It’s the mechanism that turns noise into signal.

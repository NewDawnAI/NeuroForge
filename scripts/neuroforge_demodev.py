# neuroforge_demodev.py
# NeuroForge developmental arc demo
# Chaos -> Mimicry -> Memory -> Reflection
# Now with confidence + reward visualization.

import numpy as np
import random
from collections import deque, defaultdict
import math
import time
import matplotlib.pyplot as plt

np.random.seed(0)
random.seed(0)

def normalize(x):
    s = np.linalg.norm(x)
    return x if s == 0 else x / s

# --- MemoryDB ---------------------------------------------------------------
class MemoryDB:
    def __init__(self):
        self.episodes = []
    def insert_episode(self, sensory, action, reward, embedding):
        ep = {
            'id': len(self.episodes),
            'sensory': np.array(sensory),
            'action': int(action),
            'reward': float(reward),
            'embedding': np.array(embedding),
            'timestamp': time.time()
        }
        self.episodes.append(ep)
        return ep['id']

# --- Synthetic Encoders -----------------------------------------------------
class SyntheticVision:
    def __init__(self, dim=16): self.dim = dim
    def sample(self):
        t = random.random()*10
        base = np.sin(np.linspace(0, 3.14, self.dim) + t)
        noise = np.random.normal(scale=0.6, size=self.dim)
        return normalize(base + noise)

class SyntheticAudio:
    def __init__(self, dim=12): self.dim = dim
    def sample(self):
        t = random.random()*5
        base = np.sign(np.sin(np.linspace(0, 6.28, self.dim) * (1 + 0.5*math.sin(t))))
        noise = np.random.normal(scale=0.4, size=self.dim)
        return normalize(base + noise)

# --- Region -----------------------------------------------------------------
class Region:
    def __init__(self, n_units, name='region'):
        self.n = n_units; self.name = name
        self.weights = np.random.normal(scale=0.1, size=(n_units,))
        self.pre_traces = np.zeros(n_units); self.post_traces = np.zeros(n_units)
        self.trace_decay = 0.9; self.hebb_eta = 0.05
        self.stdp_A_plus = 0.02; self.stdp_A_minus = 0.025
    def encode(self, input_vec):
        x = np.dot(input_vec, np.ones(input_vec.shape[0]) * 0.5)
        return np.tanh(self.weights * x + np.random.normal(scale=0.02, size=(self.n,)))
    def hebbian_update(self, pre_act, post_act):
        dw = self.hebb_eta * (pre_act.mean() * post_act - 0.001 * self.weights)
        self.weights = np.clip(self.weights + dw, -1.0, 1.0)
    def stdp_update(self, pre_spike_times, post_spike_times):
        def resize(vec, n):
            if len(vec) == n: return vec
            idx = np.linspace(0, len(vec) - 1, n).astype(int)
            return vec[idx]
        pre = resize(pre_spike_times, self.n); post = resize(post_spike_times, self.n)
        self.pre_traces *= self.trace_decay; self.post_traces *= self.trace_decay
        self.pre_traces += (pre > 0).astype(float); self.post_traces += (post > 0).astype(float)
        dw = self.stdp_A_plus * (self.post_traces - self.pre_traces) - self.stdp_A_minus * (self.pre_traces * 0.1)
        self.weights = np.clip(self.weights + dw, -1.0, 1.0)

# --- Motor Cortex (Q-learning) ----------------------------------------------
class MotorCortex:
    def __init__(self, n_actions=5, state_dim=8):
        self.n_actions = n_actions; self.state_dim = state_dim
        self.Q = defaultdict(lambda: np.zeros(self.n_actions))
        self.alpha = 0.2; self.gamma = 0.9; self.epsilon = 0.15
    def state_from_embedding(self, emb):
        return tuple(np.round(emb[:self.state_dim] * 3).astype(int))
    def select_action(self, state_key):
        if random.random() < self.epsilon: return random.randrange(self.n_actions)
        return int(np.argmax(self.Q[state_key]))
    def update(self, s, a, r, s_next):
        q = self.Q[s]; q_next = self.Q[s_next]
        target = r + self.gamma * q_next.max()
        q[a] += self.alpha * (target - q[a]); self.Q[s] = q

# --- Self Node --------------------------------------------------------------
class SelfNode:
    def __init__(self):
        self.confidence = {'vision': 0.1, 'audio': 0.1, 'motor': 0.1}
        self.likelihood_scale = 0.2
    def bayesian_update(self, cap, evidence_likelihood):
        prior = self.confidence.get(cap, 0.1)
        posterior = (1 - self.likelihood_scale) * prior + self.likelihood_scale * evidence_likelihood
        self.confidence[cap] = max(0.0, min(1.0, posterior))
        return posterior
    def meta_adjust(self, regions):
        for rname, region in regions.items():
            if rname == 'vision': region.hebb_eta *= (1.0 if self.confidence['vision'] > 0.2 else 1.05)
            if rname == 'audio': region.hebb_eta *= (1.0 if self.confidence['audio'] > 0.2 else 1.03)

# --- HypergraphBrain --------------------------------------------------------
class HypergraphBrain:
    def __init__(self):
        self.vision_enc = SyntheticVision(16); self.audio_enc = SyntheticAudio(12)
        self.vision = Region(8, 'vision'); self.audio = Region(6, 'audio')
        self.motor = MotorCortex(6, 6); self.selfnode = SelfNode(); self.memdb = MemoryDB()
        self.tick = 0; self.conf_history = []; self.reward_history = []
    def step(self):
        v = self.vision_enc.sample(); a = self.audio_enc.sample()
        v_act = self.vision.encode(v); a_act = self.audio.encode(a)
        embedding = normalize(np.concatenate([v_act[:4], a_act[:4]]))
        s_key = self.motor.state_from_embedding(embedding)
        action = self.motor.select_action(s_key)
        target = int((np.argmax(v_act) + np.argmax(a_act)) % self.motor.n_actions)
        reward = 1.0 if action == target else 0.2*(1.0 - abs(action - target)/max(1, self.motor.n_actions-1))
        self.vision.hebbian_update(v, v_act); self.audio.hebbian_update(a, a_act)
        self.vision.stdp_update((v > np.percentile(v,85)).astype(int),(v_act > np.percentile(v_act,85)).astype(int))
        self.audio.stdp_update((a > np.percentile(a,85)).astype(int),(a_act > np.percentile(a_act,85)).astype(int))
        self.motor.update(s_key, action, reward, self.motor.state_from_embedding(embedding))
        self.memdb.insert_episode(np.concatenate([v,a]), action, reward, normalize(np.concatenate([v[:8], a[:4]])))
        v_conf = self.selfnode.bayesian_update('vision', reward)
        a_conf = self.selfnode.bayesian_update('audio', 1.0-reward)
        m_conf = self.selfnode.bayesian_update('motor', reward)
        self.selfnode.meta_adjust({'vision':self.vision,'audio':self.audio,'motor':self.motor})
        self.conf_history.append(dict(v=v_conf,a=a_conf,m=m_conf)); self.reward_history.append(reward)
        self.tick += 1; return {'action':action,'target':target,'reward':reward}

# --- Run Demo ---------------------------------------------------------------
def run_demo(steps=200):
    brain = HypergraphBrain()
    print("=== NeuroForge developmental arc demo ===")
    for t in range(steps):
        out = brain.step()
        if t % 20 == 0:
            print(f"[tick {t}] action={out['action']} target={out['target']} reward={out['reward']:.2f}")
    print("\nFinal confidence:", brain.selfnode.confidence)
    print("Total episodes stored:", len(brain.memdb.episodes))

    # --- Plot confidence and reward
    vs = [c['v'] for c in brain.conf_history]
    au = [c['a'] for c in brain.conf_history]
    mo = [c['m'] for c in brain.conf_history]
    rewards = brain.reward_history

    # rolling average of reward
    window = 20
    reward_smooth = [np.mean(rewards[max(0,i-window):i+1]) for i in range(len(rewards))]

    fig, ax1 = plt.subplots()
    ax1.plot(vs, label="Vision")
    ax1.plot(au, label="Audio")
    ax1.plot(mo, label="Motor")
    ax1.set_xlabel("Tick")
    ax1.set_ylabel("Confidence")
    ax1.legend(loc="upper left")
    ax1.set_title("NeuroForge Identity Arc")

    ax2 = ax1.twinx()
    ax2.plot(reward_smooth, color="black", linestyle="--", label="Avg Reward")
    ax2.set_ylabel("Reward")
    ax2.legend(loc="upper right")

    plt.show()

if __name__ == '__main__':
    run_demo(steps=220)

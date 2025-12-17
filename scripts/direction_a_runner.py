import csv
import json
import math
import os
import random
from typing import List, Tuple, Dict
from encoders_clip import encode_image, encode_text
import sqlite3
import time

class Schedules:
    def __init__(self, total_steps: int):
        self.total_steps = total_steps

    def ema_alpha(self, step: int) -> float:
        a_max_start = 0.20
        a_max_end = 0.05
        t = min(step / max(1, self.total_steps), 1.0)
        return a_max_start + (a_max_end - a_max_start) * t

    def lr(self, step: int) -> float:
        lr_start = 0.05
        lr_end = 0.005
        t = min(step / max(1, self.total_steps), 1.0)
        return lr_end + (lr_start - lr_end) * (0.5 * (1 + math.cos(math.pi * (1 - t))))

    def reward_scale(self, step: int) -> float:
        warm = min(step / 20000.0, 1.0)
        return 3.0 + (1.0 - 3.0) * warm

def zscore(values: List[float]) -> List[float]:
    if not values:
        return values
    m = sum(values) / len(values)
    v = sum((x - m) ** 2 for x in values) / max(1, len(values) - 1)
    s = math.sqrt(v) if v > 0 else 1.0
    return [(x - m) / s for x in values]

def dot(a: List[float], b: List[float]) -> float:
    return sum(x * y for x, y in zip(a, b))

def ema_update(student: List[float], target: List[float], alpha: float) -> List[float]:
    return [s * (1 - alpha) + t * alpha for s, t in zip(student, target)]

class ReplayBuffer:
    def __init__(self, capacity: int):
        self.capacity = capacity
        self.items: List[Tuple[List[float], float]] = []

    def add(self, teacher: List[float], reward: float):
        self.items.append((teacher, reward))
        self.items.sort(key=lambda x: x[1], reverse=True)
        if len(self.items) > self.capacity:
            self.items = self.items[:self.capacity]

    def sample(self, n: int) -> List[List[float]]:
        n = min(n, len(self.items))
        return [t for t, _ in random.sample(self.items, n)]

def load_triplets(images_path: str, texts_path: str) -> List[Tuple[str, str]]:
    with open(images_path, 'r', encoding='utf-8') as fi, open(texts_path, 'r', encoding='utf-8') as ft:
        images = [line.strip() for line in fi if line.strip()]
        texts = [line.strip() for line in ft if line.strip()]
    n = min(len(images), len(texts))
    pairs = list(zip(images[:n], texts[:n]))
    random.shuffle(pairs)
    return pairs

def batch_pairs(pairs: List[Tuple[str, str]], batch_size: int) -> List[List[Tuple[str, str]]]:
    return [pairs[i:i+batch_size] for i in range(0, len(pairs), batch_size)]

def combine_teacher(image_path: str, caption: str) -> List[float]:
    vi = encode_image(image_path)
    vt = encode_text(caption)
    dim = min(len(vi), len(vt))
    if dim == 0:
        return []
    return _norm([(vi[i] + vt[i]) * 0.5 for i in range(dim)])

def run_training(config: Dict[str, any]):
    images = config.get('images_path', 'data/images.txt')
    texts = config.get('texts_path', 'data/text.txt')
    out_dir = config.get('output_dir', 'Artifacts/CSV/run_latest')
    memory_db = config.get('memory_db', os.environ.get('MEMDB', ''))
    memdb_interval = int(config.get('memdb_interval', 250))
    run_id = int(config.get('run_id', 1))
    os.makedirs(out_dir, exist_ok=True)
    pairs = load_triplets(images, texts)
    # de-duplicate teacher captions and enforce shuffling
    seen: set = set()
    cleaned = []
    for img, txt in pairs:
        if txt not in seen:
            seen.add(txt)
            cleaned.append((img, txt))
    random.shuffle(cleaned)
    batches = batch_pairs(cleaned, config.get('batch_size', 16))
    total_steps = min(config.get('max_steps', 50000), len(batches))
    sched = Schedules(total_steps)
    replay = ReplayBuffer(capacity=200)

    order = cleaned
    teachers: Dict[str, List[float]] = {}
    students: Dict[str, List[float]] = {}
    conn = ensure_db(memory_db)
    for img, txt in order:
        tvec = combine_teacher(img, txt)
        teachers[txt] = tvec
        rnd = [random.random() for _ in range(len(tvec))]
        students[txt] = _norm(rnd)
    norms_log = []

    with open(os.path.join(out_dir, 'reward_log.csv'), 'w', newline='') as fr, open(os.path.join(out_dir, 'similarity_log.csv'), 'w', newline='') as fs:
        rw = csv.writer(fr)
        sw = csv.writer(fs)
        rw.writerow(['step', 'reward_scale', 'mean_reward'])
        sw.writerow(['step', 'mean_similarity'])

        for step, batch in enumerate(batches[:total_steps], start=1):
            rs = sched.reward_scale(step)
            lr = sched.lr(step)
            alpha = sched.ema_alpha(step)

            tvecs = [teachers[txt] for _, txt in batch]
            sims = [dot(students[txt], teachers[txt]) for _, txt in batch]
            rewards = [rs * s for s in sims]
            rewards = zscore(rewards)

            mean_sim = sum(sims) / len(sims)
            sw.writerow([step, mean_sim])
            rw.writerow([step, rs, sum(rewards) / len(rewards)])

            for (_, txt), r in zip(batch, rewards):
                replay.add(teachers[txt], r)

            if mean_sim < 0.0 and step > 100:
                continue

            mimic_count = max(1, int(config.get('mimicry_passes', 2)))
            for _ in range(mimic_count):
                img, txt = batch[random.randrange(len(batch))]
                target = teachers[txt]
                students[txt] = ema_update(students[txt], target, alpha * lr)

            if step % config.get('replay_interval', 100) == 0:
                for tt in replay.sample(config.get('replay_count', 8)):
                    idx = random.randrange(len(order))
                    _, txt_any = order[idx]
                    students[txt_any] = ema_update(students[txt_any], tt, alpha * lr)

            nmean = sum(math.sqrt(sum(x*x for x in students[txt])) for _, txt in batch) / len(batch)
            norms_log.append((step, nmean))

            if conn and step % memdb_interval == 0:
                for img, txt in batch:
                    insert_embedding(conn, run_id, step, txt, "phase_a_teacher", teachers[txt], {"caption": txt, "img": img})
                    insert_embedding(conn, run_id, step, txt, "phase_a_student", students[txt], {"caption": txt, "img": img})

    with open(os.path.join(out_dir, 'norms.csv'), 'w', newline='') as fn:
        w = csv.writer(fn)
        w.writerow(['step', 'student_norm'])
        w.writerows(norms_log)

    with open(os.path.join(out_dir, 'teachers.csv'), 'w', newline='') as ft:
        wt = csv.writer(ft)
        wt.writerow(["emb"])
        for _, txt in order:
            wt.writerow(teachers[txt])
    with open(os.path.join(out_dir, 'students.csv'), 'w', newline='') as fs2:
        ws2 = csv.writer(fs2)
        ws2.writerow(["emb"])
        for _, txt in order:
            ws2.writerow(students[txt])

    if conn:
        conn.close()

if __name__ == '__main__':
    cfg_path = os.environ.get('DIRECTION_A_CONFIG', '')
    cfg = {}
    if cfg_path and os.path.exists(cfg_path):
        with open(cfg_path, 'r', encoding='utf-8') as f:
            cfg = json.load(f)
    run_training(cfg)
def _norm(v: List[float]) -> List[float]:
    s = math.sqrt(sum(x*x for x in v))
    if s <= 1e-9:
        return v
    return [x / s for x in v]

def ensure_db(path: str):
    if not path:
        return None
    conn = sqlite3.connect(path, timeout=30)
    conn.execute('PRAGMA journal_mode=WAL;')
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS embeddings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            run_id INTEGER,
            step INTEGER,
            ts_ms INTEGER,
            content_id TEXT,
            state_type TEXT,
            dim INTEGER,
            vec_json TEXT,
            meta_json TEXT
        )
        """
    )
    conn.execute("CREATE INDEX IF NOT EXISTS idx_embeddings_run_step ON embeddings(run_id, step)")
    conn.execute("CREATE INDEX IF NOT EXISTS idx_embeddings_content ON embeddings(content_id)")
    conn.commit()
    return conn

def insert_embedding(conn, run_id: int, step: int, content_id: str, state_type: str, vec: List[float], meta: Dict[str, any]):
    if conn is None:
        return
    ts_ms = int(time.time() * 1000)
    dim = len(vec)
    vec_json = json.dumps({"vec": vec})
    meta_json = json.dumps(meta or {})
    conn.execute(
        """
        INSERT INTO embeddings (run_id, step, ts_ms, content_id, state_type, dim, vec_json, meta_json)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (run_id, step, ts_ms, content_id, state_type, dim, vec_json, meta_json),
    )
    conn.commit()

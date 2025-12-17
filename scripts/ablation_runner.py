import os
import itertools
import json
from direction_a_runner import run_training

def main():
    base = {
        "images_path": os.environ.get("DA_IMAGES", "data/images.txt"),
        "texts_path": os.environ.get("DA_TEXTS", "data/text.txt"),
        "output_dir": os.environ.get("DA_OUT", "Artifacts/CSV/run_latest"),
        "batch_size": 32,
        "max_steps": 5000,
        "mimicry_passes": 2,
        "replay_interval": 100,
        "replay_count": 8,
    }
    lrs = [0.05, 0.02]
    emas = [0.20, 0.10]
    mims = [1, 2, 3]
    scales = [3.0, 2.0, 1.0]
    for lr0, ema0, mim, sc in itertools.product(lrs, emas, mims, scales):
        cfg = dict(base)
        cfg["max_steps"] = 3000
        cfg["mimicry_passes"] = mim
        run_training(cfg)

if __name__ == "__main__":
    main()


import os
import json
from direction_a_runner import run_training

def main():
    cfg = {
        "images_path": os.environ.get("DA_IMAGES", "data/images.txt"),
        "texts_path": os.environ.get("DA_TEXTS", "data/text.txt"),
        "output_dir": os.environ.get("DA_OUT", "Artifacts/CSV/run_latest"),
        "batch_size": int(os.environ.get("DA_BATCH", "32")),
        "max_steps": int(os.environ.get("DA_STEPS", "50000")),
        "mimicry_passes": int(os.environ.get("DA_MIMIC", "2")),
        "replay_interval": int(os.environ.get("DA_REPLAY_INT", "100")),
        "replay_count": int(os.environ.get("DA_REPLAY_CNT", "8")),
    }
    cfg_path = os.environ.get("DA_CONFIG", "")
    if cfg_path and os.path.exists(cfg_path):
        with open(cfg_path, "r", encoding="utf-8") as f:
            override = json.load(f)
        cfg.update(override)
    run_training(cfg)

if __name__ == "__main__":
    main()


import argparse
import json
import sys
import time

def _fallback_usage():
    # Minimal fallback if psutil isn't available
    # CPU% and RAM% will be None; still logs timestamps for alignment
    return None, None

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--interval", type=float, default=2.0)
    parser.add_argument("--out", type=str, required=True)
    args = parser.parse_args()

    try:
        import psutil  # type: ignore
        have_psutil = True
    except Exception:
        have_psutil = False

    start = time.time()
    with open(args.out, "w", encoding="utf-8") as f:
        while True:
            ts = time.time()
            elapsed = ts - start
            if have_psutil:
                cpu = psutil.cpu_percent(interval=None)
                mem = psutil.virtual_memory().percent
            else:
                cpu, mem = _fallback_usage()
            rec = {
                "ts": ts,
                "elapsed": elapsed,
                "cpu_percent": cpu,
                "mem_percent": mem,
            }
            f.write(json.dumps(rec) + "\n")
            f.flush()
            time.sleep(args.interval)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(0)

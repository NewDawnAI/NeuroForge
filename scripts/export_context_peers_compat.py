import argparse
import json
import os

def _load_json(path: str):
    with open(path, 'r', encoding='utf-8') as f:
        return json.load(f)

def _write_json(path: str, obj: dict):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(obj, f, ensure_ascii=False, indent=2)

def merge_couplings_into_alias(run_meta_path: str, alias_path: str):
    meta = _load_json(run_meta_path)
    alias = {}
    if os.path.exists(alias_path):
        try:
            alias = _load_json(alias_path)
        except Exception:
            alias = {}

    ctx = meta.get('context', {}) if isinstance(meta.get('context', {}), dict) else {}
    kappa = ctx.get('kappa')
    enabled = ctx.get('couplings_enabled')
    couplings = ctx.get('couplings') if isinstance(ctx.get('couplings'), list) else []
    peers = ctx.get('peers') if isinstance(ctx.get('peers'), list) else []

    # Ensure top-level fields exist in alias
    alias.setdefault('export_time', int(meta.get('ts_ms', meta.get('export_time', 0)) or 0))
    alias.setdefault('peers', alias.get('peers', {}))

    # Attach a config/meta section that dashboards can read
    config = alias.get('config', alias.get('meta', {}))
    if not isinstance(config, dict):
        config = {}
    config['kappa'] = kappa
    config['couplings_enabled'] = bool(enabled)
    # Prefer couplings_preview as requested
    config['couplings_preview'] = couplings
    config['peers'] = peers
    alias['config'] = config

    _write_json(alias_path, alias)
    return alias_path

def main():
    parser = argparse.ArgumentParser(description='Merge run_meta.context couplings into context_peer_stream alias JSON')
    parser.add_argument('--meta', required=True, help='Path to run_meta.json (exports directory)')
    parser.add_argument('--alias', required=True, help='Path to pages/tags/runner/context_peer_stream.json')
    args = parser.parse_args()

    out = merge_couplings_into_alias(args.meta, args.alias)
    print(f"Updated alias: {out}")

if __name__ == '__main__':
    main()

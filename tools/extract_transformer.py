#!/usr/bin/env python3
"""
Extract embeddings from Transformer models (text or CLIP vision) and save JSON compatible with tools/analyze.py.

Usage examples:
  # Text (LLM/encoder)
  python tools/extract_transformer.py --model bert-base-uncased --inputs data/text.txt --out emb_text.json --layer -1

  # All layers (text)
  python tools/extract_transformer.py --model gpt2 --inputs data/text.txt --out emb_all_layers.json --all-layers

  # Vision (CLIP)
  python tools/extract_transformer.py --model openai/clip-vit-base-patch32 --inputs data/images.txt --out emb_clip.json --modality vision

Inputs:
  --inputs is a text file; for modality=text, each line is an input string.
  For modality=vision, each line is a path to an image file.

Outputs:
  If --all-layers: { "layers": { "0": [[...], ...], "1": [[...], ...], ... }, "labels": [...] }
  Else:           { "vectors": [[...], ...], "labels": [...] }
"""
import argparse, json, os, sys

def extract_text(model_name: str, inputs_path: str, out_path: str, layer: int, all_layers: bool):
    import torch
    from transformers import AutoTokenizer, AutoModel
    tok = AutoTokenizer.from_pretrained(model_name)
    mdl = AutoModel.from_pretrained(model_name, output_hidden_states=True)
    mdl.eval()
    texts = [l.strip() for l in open(inputs_path, 'r', encoding='utf-8') if l.strip()]
    labels = []
    all_vecs = []
    layers_map = {}
    with torch.no_grad():
        for t in texts:
            toks = tok(t, return_tensors="pt", truncation=True)
            out = mdl(**toks)
            hs = out.hidden_states  # tuple: (embeddings, layer1, ...)
            if all_layers:
                for li, h in enumerate(hs):
                    v = h.mean(dim=1).squeeze().tolist()  # mean-pool across tokens
                    layers_map.setdefault(str(li), []).append(v)
            else:
                use_idx = layer if layer is not None else -1
                v = hs[use_idx].mean(dim=1).squeeze().tolist()
                all_vecs.append(v)
            labels.append(t)
    if all_layers:
        json.dump({"layers": layers_map, "labels": labels}, open(out_path, "w", encoding='utf-8'))
    else:
        json.dump({"vectors": all_vecs, "labels": labels}, open(out_path, "w", encoding='utf-8'))
    print(f"Saved {len(labels)} samples → {out_path}")

def extract_clip_vision(model_name: str, inputs_path: str, out_path: str):
    import torch
    from PIL import Image
    from transformers import CLIPProcessor, CLIPModel
    processor = CLIPProcessor.from_pretrained(model_name)
    model = CLIPModel.from_pretrained(model_name)
    model.eval()
    paths = [l.strip() for l in open(inputs_path, 'r', encoding='utf-8') if l.strip()]
    labels = []
    vectors = []
    with torch.no_grad():
        for p in paths:
            try:
                image = Image.open(p).convert('RGB')
            except Exception as e:
                print(f"[extract] WARN: skipping image '{p}': {e}")
                continue
            try:
                inputs = processor(images=image, return_tensors="pt")
                feats = model.get_image_features(**inputs)  # [1, d]
                v = feats.squeeze().tolist()
                vectors.append(v)
                labels.append(os.path.basename(p))
            except Exception as e:
                print(f"[extract] WARN: failed to encode '{p}': {e}")
                continue
    json.dump({"vectors": vectors, "labels": labels}, open(out_path, "w", encoding='utf-8'))
    print(f"Saved {len(labels)} image vectors → {out_path}")

def main():
    ap = argparse.ArgumentParser(description="Transformer/CLIP embeddings extractor")
    ap.add_argument('--model', default='bert-base-uncased')
    ap.add_argument('--inputs', required=True)
    ap.add_argument('--out', default='emb.json')
    ap.add_argument('--layer', type=int, default=-1, help='hidden state index (-1 last)')
    ap.add_argument('--all-layers', action='store_true', help='export embeddings for all layers')
    ap.add_argument('--modality', choices=['text', 'vision'], default='text')
    args = ap.parse_args()

    if args.modality == 'vision' or 'clip' in args.model.lower():
        extract_clip_vision(args.model, args.inputs, args.out)
    else:
        extract_text(args.model, args.inputs, args.out, args.layer, args.all_layers)

if __name__ == '__main__':
    main()

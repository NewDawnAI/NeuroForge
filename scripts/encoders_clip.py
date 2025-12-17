import math
import os

_OPENCLIP = None
_TORCH = None
_TOKENIZER = None
_MODEL = None
_PREPROCESS = None
_PIL = None

def _try_load():
    global _OPENCLIP, _TORCH, _TOKENIZER, _MODEL, _PREPROCESS, _PIL
    if _MODEL is not None:
        return True
    try:
        import open_clip
        import torch
        from PIL import Image
        _OPENCLIP = open_clip
        _TORCH = torch
        _PIL = Image
        model_name = os.environ.get("NF_CLIP_MODEL", "ViT-B-32")
        pretrained = os.environ.get("NF_CLIP_PRETRAINED", "laion2b_s34b_b79k")
        _MODEL, _, _PREPROCESS = open_clip.create_model_and_transforms(model_name, pretrained=pretrained)
        _TOKENIZER = open_clip.get_tokenizer(model_name)
        _MODEL.eval()
        return True
    except Exception:
        return False

def _norm(v):
    s = math.sqrt(sum(x*x for x in v))
    if s <= 1e-9:
        return v
    return [x / s for x in v]

def _placeholder(seed_text: str, dim: int = 512):
    h = 1469598103934665603
    for c in seed_text:
        h ^= ord(c)
        h *= 1099511628211
        h &= (1<<64) - 1
    return _norm([((h + i*1315423911) & 0xffff)/65535.0 for i in range(dim)])

def encode_text(text: str):
    if _try_load():
        with _TORCH.no_grad():
            tokens = _TOKENIZER([text])
            emb = _MODEL.encode_text(tokens)
            vec = emb.detach().cpu().view(-1).tolist()
            return _norm(vec)
    return _placeholder("text:"+text)

def encode_image(path: str):
    if _try_load():
        with _TORCH.no_grad():
            img = _PIL.open(path).convert("RGB")
            img_t = _PREPROCESS(img).unsqueeze(0)
            emb = _MODEL.encode_image(img_t)
            vec = emb.detach().cpu().view(-1).tolist()
            return _norm(vec)
    return _placeholder("image:"+path)

if __name__ == "__main__":
    ok = _try_load()
    if not ok:
        print("encoder: fallback (open_clip/torch/PIL not loaded)")
    else:
        name = os.environ.get("NF_CLIP_MODEL", "ViT-B-32")
        try:
            # probe dims using a dummy text
            with _TORCH.no_grad():
                tokens = _TOKENIZER(["hello world"])
                emb = _MODEL.encode_text(tokens)
                dim = emb.detach().cpu().view(-1).shape[0]
        except Exception:
            dim = -1
        print(f"encoder: {name}")
        print(f"dim: {dim}")

import os
from typing import List, Tuple, Dict, Iterable, Optional, Union
from collections import OrderedDict

import numpy as np

# Optional image dependency (present elsewhere in repo)
try:
    from PIL import Image  # type: ignore
    _HAS_PIL = True
except Exception:
    _HAS_PIL = False

import wave
from contextlib import closing


class TripletDataset:
    """
    Windows-friendly triplet dataset loader for NeuroForge.

    - Expects the following subfolders inside root_dir:
        images/  -> image files (.png preferred; .jpg/.jpeg also supported)
        texts/   -> text files  (.txt)
        audio/   -> audio files (.wav)

    - Matches triplets by shared base filename (e.g., 00000.png, 00000.txt, 00000.wav)
    - Skips incomplete triplets automatically
    - Returns ordered triplets (numeric sort if possible)

    Supports three return modes via `return_mode`:
      - 'paths'   -> (img_path, txt_path, audio_path)
      - 'decoded' -> (image_array [H,W,C uint8], text_string, (audio_waveform float32, sample_rate))
      - 'both'    -> {'paths': (..), 'data': (..)}

    To reduce repeated disk I/O during training, a small LRU cache is used when decoding.
    """

    def __init__(
        self,
        root_dir: str,
        images_dir_name: str = "images",
        texts_dir_name: str = "texts",
        audio_dir_name: str = "audio",
        image_exts: Tuple[str, ...] = (".png", ".jpg", ".jpeg"),
        text_ext: str = ".txt",
        audio_ext: str = ".wav",
        limit: Optional[int] = None,
        return_mode: str = "paths",  # 'paths' | 'decoded' | 'both'
        decode_on_init: bool = False,
        cache_max_items: int = 512,
        audio_mono: bool = True,
    ) -> None:
        self.root_dir = os.path.abspath(root_dir)
        self.images_dir = os.path.join(self.root_dir, images_dir_name)
        self.texts_dir = os.path.join(self.root_dir, texts_dir_name)
        self.audios_dir = os.path.join(self.root_dir, audio_dir_name)
        self.image_exts = tuple(ext.lower() for ext in image_exts)
        self.text_ext = text_ext.lower()
        self.audio_ext = audio_ext.lower()
        self.return_mode = return_mode
        self.decode_on_init = decode_on_init
        self.audio_mono = audio_mono

        # Tiny LRU caches to avoid repeated I/O during training
        self._image_cache: "OrderedDict[str, np.ndarray]" = OrderedDict()
        self._text_cache: "OrderedDict[str, str]" = OrderedDict()
        self._audio_cache: "OrderedDict[str, Tuple[np.ndarray, int]]" = OrderedDict()
        self._cache_max_items = max(8, int(cache_max_items))

        self._validate_dirs()

        # Build maps from base name -> full path for each modality
        image_map = self._scan_dir_for_basenames(self.images_dir, self.image_exts)
        text_map = self._scan_dir_for_basenames(self.texts_dir, (self.text_ext,))
        audio_map = self._scan_dir_for_basenames(self.audios_dir, (self.audio_ext,))

        # Only keep basenames that exist in all three
        common_basenames = sorted(
            set(image_map.keys()) & set(text_map.keys()) & set(audio_map.keys()),
            key=self._numeric_key
        )

        if limit is not None and limit > 0:
            common_basenames = common_basenames[:limit]

        # Store paths per sample (keeps memory small; decode later on demand)
        self._paths: List[Tuple[str, str, str]] = [
            (image_map[b], text_map[b], audio_map[b]) for b in common_basenames
        ]

        # Optional eager decode (can be heavy; generally leave False on large sets)
        if self.decode_on_init and self.return_mode in ("decoded", "both"):
            for (ip, tp, ap) in self._paths:
                _ = self._get_decoded(ip, tp, ap)  # warm caches

        print(f"Loaded {len(self._paths)} triplets from {self.root_dir}")

    def __len__(self) -> int:
        return len(self._paths)

    def __getitem__(self, idx: int) -> Union[Tuple[str, str, str], Tuple[np.ndarray, str, Tuple[np.ndarray, int]], Dict[str, object]]:
        img_path, txt_path, audio_path = self._paths[idx]
        if self.return_mode == "paths":
            return (img_path, txt_path, audio_path)
        elif self.return_mode == "decoded":
            return self._get_decoded(img_path, txt_path, audio_path)
        elif self.return_mode == "both":
            data = self._get_decoded(img_path, txt_path, audio_path)
            return {"paths": (img_path, txt_path, audio_path), "data": data}
        else:
            raise ValueError(f"Unknown return_mode: {self.return_mode}")

    def __iter__(self) -> Iterable[Union[Tuple[str, str, str], Tuple[np.ndarray, str, Tuple[np.ndarray, int]], Dict[str, object]]]:
        for i in range(len(self)):
            yield self[i]

    # ------------------------
    # Internal helpers
    # ------------------------

    def _validate_dirs(self) -> None:
        missing = [p for p in [self.images_dir, self.texts_dir, self.audios_dir] if not os.path.isdir(p)]
        if missing:
            details = "\n".join(f" - {p}" for p in missing)
            raise FileNotFoundError(
                "Expected dataset subdirectories not found:\n" + details +
                "\nPlease ensure your layout is:\n"
                "  <root>/images/*.png (or .jpg/.jpeg)\n"
                "  <root>/texts/*.txt\n"
                "  <root>/audio/*.wav\n"
            )

    @staticmethod
    def _scan_dir_for_basenames(directory: str, valid_exts: Tuple[str, ...]) -> Dict[str, str]:
        valid_exts = tuple(ext.lower() for ext in valid_exts)
        mapping: Dict[str, str] = {}
        try:
            for fname in os.listdir(directory):
                fpath = os.path.join(directory, fname)
                if not os.path.isfile(fpath):
                    continue
                _, ext = os.path.splitext(fname)
                ext = ext.lower()
                if ext not in valid_exts:
                    continue
                base, _ = os.path.splitext(fname)
                mapping[base] = fpath
        except FileNotFoundError:
            # Handled by validation earlier; keep robust
            pass
        return mapping

    @staticmethod
    def _numeric_key(basename: str):
        # Try numeric sort (e.g., "00000" -> 0). Fallback to lexicographic.
        try:
            return int(basename)
        except ValueError:
            return basename

    # --------- Caching helpers ---------
    def _cache_get(self, cache: "OrderedDict", key: str):
        if key in cache:
            cache.move_to_end(key)
            return cache[key]
        return None

    def _cache_put(self, cache: "OrderedDict", key: str, value):
        cache[key] = value
        cache.move_to_end(key)
        if len(cache) > self._cache_max_items:
            cache.popitem(last=False)  # evict LRU

    # --------- Decoders ---------
    def _get_decoded(self, img_path: str, txt_path: str, audio_path: str) -> Tuple[np.ndarray, str, Tuple[np.ndarray, int]]:
        img = self._cache_get(self._image_cache, img_path)
        if img is None:
            img = self._load_image(img_path)
            self._cache_put(self._image_cache, img_path, img)

        txt = self._cache_get(self._text_cache, txt_path)
        if txt is None:
            txt = self._load_text(txt_path)
            self._cache_put(self._text_cache, txt_path, txt)

        aud = self._cache_get(self._audio_cache, audio_path)
        if aud is None:
            aud = self._load_audio(audio_path)
            self._cache_put(self._audio_cache, audio_path, aud)

        return img, txt, aud

    def _load_image(self, path: str) -> np.ndarray:
        if not _HAS_PIL:
            raise RuntimeError("Pillow (PIL) is required to decode images. Please install 'Pillow'.")
        with Image.open(path) as im:
            im = im.convert('RGB')
            arr = np.array(im, dtype=np.uint8)
        return arr  # H x W x C (uint8)

    @staticmethod
    def _load_text(path: str) -> str:
        with open(path, 'r', encoding='utf-8', errors='replace') as f:
            return f.read()

    def _load_audio(self, path: str) -> Tuple[np.ndarray, int]:
        with closing(wave.open(path, 'rb')) as wf:
            n_channels = wf.getnchannels()
            sample_width = wf.getsampwidth()
            sample_rate = wf.getframerate()
            n_frames = wf.getnframes()
            raw = wf.readframes(n_frames)

        # Convert raw PCM to numpy
        if sample_width == 1:
            dtype = np.uint8  # unsigned 8-bit
            offset = -128.0
            scale = 1.0 / 128.0
        elif sample_width == 2:
            dtype = np.int16
            offset = 0.0
            scale = 1.0 / 32768.0
        elif sample_width == 4:
            dtype = np.int32
            offset = 0.0
            scale = 1.0 / 2147483648.0
        else:
            raise ValueError(f"Unsupported WAV sample width: {sample_width} bytes")

        audio = np.frombuffer(raw, dtype=dtype).astype(np.float32)
        if sample_width == 1:
            audio = (audio + offset) * scale
        else:
            audio = (audio - offset) * scale

        if n_channels > 1:
            audio = audio.reshape(-1, n_channels)
            if self.audio_mono:
                audio = audio.mean(axis=1)
        return audio, sample_rate


# ------------------------
# Example usage
# ------------------------
if __name__ == "__main__":
    DATA_DIR = r"C:\Users\ashis\Desktop\NeuroForge\flickr30k_triplets"
    # Return decoded items and limit for a quick sanity check
    dataset = TripletDataset(DATA_DIR, limit=5, return_mode='decoded')

    print(f"Total triplets: {len(dataset)}")
    for i in range(min(3, len(dataset))):
        img, txt, (wav, sr) = dataset[i]
        print(f"[{i}] Image: {img.shape}, Text chars: {len(txt)}, Audio: {wav.shape} @ {sr} Hz")
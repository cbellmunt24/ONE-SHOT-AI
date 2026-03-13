"""
01b_add_windowed_features.py — Adds windowed_features to existing JSONs
Only loads audio + computes windowed_features, skips full re-extraction.

Uso:
    python 01b_add_windowed_features.py
"""

import os
import sys
import json
from pathlib import Path
from tqdm import tqdm

sys.path.insert(0, os.path.dirname(__file__))
from utils.audio_features import load_audio, extract_windowed_features, SAMPLE_RATE, MAX_DURATION_S

SCRIPT_DIR = Path(__file__).parent
TRAINING_DIR = SCRIPT_DIR.parent
LIBRARIES_DIR = TRAINING_DIR / "libraries"
FEATURES_DIR = TRAINING_DIR / "data" / "features"

INSTRUMENTS = ["kicks", "snares", "hihats", "claps", "percs",
               "808s", "leads", "plucks", "pads", "textures"]

GENRES = ["trap", "hiphop", "techno", "house", "reggaeton",
          "afrobeat", "rnb", "edm", "ambient"]


def main():
    # Find all JSONs missing windowed_features
    to_update = []
    for inst in INSTRUMENTS:
        for genre in GENRES:
            feat_dir = FEATURES_DIR / f"{inst}_{genre}"
            if not feat_dir.exists():
                continue
            for json_path in sorted(feat_dir.glob("*.json")):
                if json_path.name.startswith("_"):
                    continue
                try:
                    with open(json_path) as f:
                        data = json.load(f)
                    if "windowed_features" not in data:
                        # Find corresponding audio file
                        sample_dir = LIBRARIES_DIR / inst / genre
                        stem = json_path.stem
                        audio_path = None
                        for ext in ['.wav', '.WAV', '.aif', '.aiff', '.flac']:
                            candidate = sample_dir / f"{stem}{ext}"
                            if candidate.exists():
                                audio_path = candidate
                                break
                        if audio_path:
                            to_update.append((json_path, audio_path, data))
                except Exception:
                    continue

    print(f"JSONs sin windowed_features: {len(to_update)}")
    if not to_update:
        print("Nada que hacer.")
        return

    updated = 0
    errors = 0
    for json_path, audio_path, data in tqdm(to_update, desc="Adding windowed_features"):
        try:
            y = load_audio(str(audio_path), SAMPLE_RATE)
            if len(y) == 0 or len(y) / SAMPLE_RATE > MAX_DURATION_S:
                continue
            wf = extract_windowed_features(y, SAMPLE_RATE)
            data["windowed_features"] = wf
            with open(json_path, 'w') as f:
                json.dump(data, f, indent=2)
            updated += 1
        except Exception as e:
            print(f"  [error] {json_path.name}: {e}")
            errors += 1

    print(f"\nActualizados: {updated}")
    print(f"Errores: {errors}")


if __name__ == "__main__":
    main()

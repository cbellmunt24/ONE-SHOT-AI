"""
01_extract_features.py — Capa 1: Extraccion de features de audio
Analiza cada sample en Training/libraries/ y genera JSONs de features.

Uso:
    python 01_extract_features.py
    python 01_extract_features.py --instrument kicks
    python 01_extract_features.py --instrument kicks --genre trap
"""

import os
import sys
import json
import argparse
from pathlib import Path
from tqdm import tqdm

# Asegurar imports locales
sys.path.insert(0, os.path.dirname(__file__))
from utils.audio_features import extract_all_features


# Rutas base
SCRIPT_DIR = Path(__file__).parent
TRAINING_DIR = SCRIPT_DIR.parent
LIBRARIES_DIR = TRAINING_DIR / "libraries"
FEATURES_DIR = TRAINING_DIR / "data" / "features"

INSTRUMENTS = ["kicks", "snares", "hihats", "claps", "percs",
               "808s", "leads", "plucks", "pads", "textures"]

GENRES = ["trap", "hiphop", "techno", "house", "reggaeton",
          "afrobeat", "rnb", "edm", "ambient"]


def find_audio_files(directory: Path) -> list:
    """Busca archivos de audio (.wav, .WAV, .aif, .aiff) en un directorio."""
    extensions = {'.wav', '.WAV', '.aif', '.aiff', '.flac'}
    files = []
    if directory.exists():
        for f in directory.iterdir():
            if f.suffix in extensions and f.is_file() and not f.name.startswith("._"):
                files.append(f)
    return sorted(files)


def _json_needs_update(path: Path) -> bool:
    """Comprueba si un JSON existente necesita re-extraccion (sin windowed_features)."""
    try:
        with open(path) as f:
            data = json.load(f)
        return "windowed_features" not in data
    except Exception:
        return True


def process_instrument_genre(instrument: str, genre: str,
                              force_reextract: bool = False) -> dict:
    """Procesa todos los samples de un instrumento/genero."""
    sample_dir = LIBRARIES_DIR / instrument / genre
    audio_files = find_audio_files(sample_dir)

    if not audio_files:
        return {"skipped": True, "reason": "no audio files found"}

    output_dir = FEATURES_DIR / f"{instrument}_{genre}"
    output_dir.mkdir(parents=True, exist_ok=True)

    results = {
        "instrument": instrument,
        "genre": genre,
        "num_samples": len(audio_files),
        "processed": 0,
        "errors": 0,
        "files": []
    }

    print(f"\n  {instrument}/{genre}: {len(audio_files)} samples encontrados")

    for audio_path in tqdm(audio_files, desc=f"    {instrument}/{genre}",
                           leave=False):
        # Skip si ya existe el JSON de features (y tiene windowed_features)
        output_path = output_dir / f"{audio_path.stem}.json"
        if output_path.exists():
            if not force_reextract or not _json_needs_update(output_path):
                results["processed"] += 1
                results["files"].append(audio_path.name)
                continue

        try:
            features = extract_all_features(str(audio_path))

            if "error" in features:
                print(f"    [warn] {audio_path.name}: {features['error']}")
                results["errors"] += 1
                continue

            # Guardar features individuales
            features["source_file"] = audio_path.name
            features["instrument"] = instrument
            features["genre"] = genre

            with open(output_path, 'w') as f:
                json.dump(features, f, indent=2)

            results["processed"] += 1
            results["files"].append(audio_path.name)

        except Exception as e:
            print(f"    [error] {audio_path.name}: {e}")
            results["errors"] += 1

    print(f"    -> {results['processed']}/{results['num_samples']} procesados"
          f" ({results['errors']} errores)")

    return results


def main():
    parser = argparse.ArgumentParser(
        description="Extrae features de audio de las librerias de samples")
    parser.add_argument("--instrument", type=str, default=None,
                        help="Procesar solo este instrumento")
    parser.add_argument("--genre", type=str, default=None,
                        help="Procesar solo este genero")
    parser.add_argument("--force-reextract", action="store_true",
                        help="Re-extraer JSONs que no tienen windowed_features")
    args = parser.parse_args()

    instruments = [args.instrument] if args.instrument else INSTRUMENTS
    genres = [args.genre] if args.genre else GENRES

    print("=" * 60)
    print("ONE-SHOT AI — Feature Extraction (Capa 1)")
    print("=" * 60)
    print(f"Libraries dir: {LIBRARIES_DIR}")
    print(f"Output dir:    {FEATURES_DIR}")

    # Verificar que hay samples
    total_samples = 0
    for inst in instruments:
        for genre in genres:
            sample_dir = LIBRARIES_DIR / inst / genre
            total_samples += len(find_audio_files(sample_dir))

    if total_samples == 0:
        print("\n[!] No se encontraron samples en Training/libraries/")
        print("    Deposita archivos .wav en las carpetas correspondientes:")
        print("    Training/libraries/{instrumento}/{genero}/")
        return

    print(f"\nTotal samples a procesar: {total_samples}")

    # Procesar
    all_results = []
    for inst in instruments:
        print(f"\n{'-' * 40}")
        print(f"Instrumento: {inst}")
        for genre in genres:
            result = process_instrument_genre(inst, genre,
                                                force_reextract=args.force_reextract)
            if not result.get("skipped"):
                all_results.append(result)

    # Resumen final
    print(f"\n{'=' * 60}")
    print("RESUMEN")
    print(f"{'=' * 60}")
    total_processed = sum(r["processed"] for r in all_results)
    total_errors = sum(r["errors"] for r in all_results)
    print(f"Combinaciones procesadas: {len(all_results)}")
    print(f"Samples procesados:       {total_processed}")
    print(f"Errores:                  {total_errors}")
    print(f"\nFeatures guardadas en: {FEATURES_DIR}")

    # Guardar resumen
    summary_path = FEATURES_DIR / "_extraction_summary.json"
    with open(summary_path, 'w') as f:
        json.dump({
            "total_processed": total_processed,
            "total_errors": total_errors,
            "combinations": len(all_results),
            "details": all_results
        }, f, indent=2)
    print(f"Resumen guardado en: {summary_path}")


if __name__ == "__main__":
    main()

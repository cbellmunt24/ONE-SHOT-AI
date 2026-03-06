"""
02_build_profiles.py — Capa 1: Construccion de perfiles timbricos
Agrega las features individuales en perfiles estadisticos por instrumento/genero.

Uso:
    python 02_build_profiles.py
    python 02_build_profiles.py --plots
"""

import os
import sys
import json
import argparse
import numpy as np
from pathlib import Path

sys.path.insert(0, os.path.dirname(__file__))
from utils.visualization import generate_all_plots


SCRIPT_DIR = Path(__file__).parent
TRAINING_DIR = SCRIPT_DIR.parent
FEATURES_DIR = TRAINING_DIR / "data" / "features"
PROFILES_DIR = TRAINING_DIR / "data" / "profiles"
PLOTS_DIR = TRAINING_DIR / "data" / "plots"

# Features escalares para agregar estadisticamente
SCALAR_FEATURES = [
    "attack_time_ms", "decay_time_ms", "total_duration_ms",
    "peak_amplitude", "crest_factor", "transient_sharpness",
    "spectral_centroid", "spectral_rolloff_85", "spectral_bandwidth",
    "spectral_flatness", "spectral_tilt",
    "fundamental_freq", "harmonic_ratio",
    "pitch_drop_semitones", "pitch_drop_time_ms",
    "brightness_index", "warmth_index", "noise_ratio",
    "stereo_width", "zero_crossing_rate",
]


def compute_stats(values: list) -> dict:
    """Calcula estadisticas de una lista de valores."""
    arr = np.array(values)
    return {
        "mean": float(np.mean(arr)),
        "median": float(np.median(arr)),
        "std": float(np.std(arr)),
        "min": float(np.min(arr)),
        "max": float(np.max(arr)),
        "q25": float(np.percentile(arr, 25)),
        "q75": float(np.percentile(arr, 75)),
    }


def build_profile(feature_dir: Path) -> dict:
    """Construye perfil estadistico a partir de JSONs de features."""
    json_files = sorted(feature_dir.glob("*.json"))
    if not json_files:
        return None

    # Cargar todas las features
    all_features = []
    for jf in json_files:
        with open(jf, 'r') as f:
            data = json.load(f)
        if "error" not in data:
            all_features.append(data)

    if not all_features:
        return None

    instrument = all_features[0].get("instrument", "unknown")
    genre = all_features[0].get("genre", "unknown")

    profile = {
        "instrument": instrument,
        "genre": genre,
        "num_samples_analyzed": len(all_features),
        "profile": {}
    }

    # Estadisticas de features escalares
    for feat_name in SCALAR_FEATURES:
        values = [f[feat_name] for f in all_features if feat_name in f]
        if values:
            profile["profile"][feat_name] = compute_stats(values)

    # Estadisticas de MFCC (por coeficiente)
    mfcc_lists = [f["mfcc"] for f in all_features if "mfcc" in f]
    if mfcc_lists:
        n_mfcc = len(mfcc_lists[0])
        mfcc_stats = []
        for i in range(n_mfcc):
            vals = [m[i] for m in mfcc_lists if len(m) > i]
            mfcc_stats.append(compute_stats(vals))
        profile["profile"]["mfcc_stats"] = mfcc_stats

    # Envolvente RMS media
    rms_envs = [f["rms_envelope"] for f in all_features
                if "rms_envelope" in f]
    if rms_envs:
        max_len = max(len(e) for e in rms_envs)
        padded = np.zeros((len(rms_envs), max_len))
        for i, env in enumerate(rms_envs):
            padded[i, :len(env)] = env
        profile["profile"]["rms_envelope_mean"] = np.mean(padded, axis=0).tolist()
        profile["profile"]["rms_envelope_std"] = np.std(padded, axis=0).tolist()

    return profile


def main():
    parser = argparse.ArgumentParser(
        description="Construye perfiles timbricos a partir de features")
    parser.add_argument("--plots", action="store_true",
                        help="Generar graficas comparativas")
    args = parser.parse_args()

    PROFILES_DIR.mkdir(parents=True, exist_ok=True)

    print("=" * 60)
    print("ONE-SHOT AI — Build Profiles (Capa 1)")
    print("=" * 60)

    # Buscar directorios de features
    feature_dirs = [d for d in FEATURES_DIR.iterdir()
                    if d.is_dir() and not d.name.startswith("_")]

    if not feature_dirs:
        print("\n[!] No se encontraron features extraidas.")
        print("    Ejecuta primero: python 01_extract_features.py")
        return

    print(f"Combinaciones encontradas: {len(feature_dirs)}")

    profiles_built = 0
    for fd in sorted(feature_dirs):
        profile = build_profile(fd)
        if profile is None:
            print(f"  [skip] {fd.name}: sin features validas")
            continue

        output_path = PROFILES_DIR / f"{fd.name}_profile.json"
        with open(output_path, 'w') as f:
            json.dump(profile, f, indent=2)

        n = profile["num_samples_analyzed"]
        print(f"  [ok] {fd.name}: perfil construido ({n} samples)")
        profiles_built += 1

    print(f"\n{'=' * 60}")
    print(f"Perfiles construidos: {profiles_built}")
    print(f"Guardados en: {PROFILES_DIR}")

    # Generar graficas
    if args.plots and profiles_built > 0:
        print(f"\nGenerando graficas en: {PLOTS_DIR}")
        generate_all_plots(str(PROFILES_DIR), str(PLOTS_DIR))

    # Generar CSV resumen
    if profiles_built > 0:
        _export_summary_csv(PROFILES_DIR)


def _export_summary_csv(profiles_dir: Path):
    """Exporta un CSV resumen con las medias de todas las combinaciones."""
    import csv

    csv_path = profiles_dir / "_all_profiles_summary.csv"
    profiles = sorted(profiles_dir.glob("*_profile.json"))

    if not profiles:
        return

    # Leer primer perfil para obtener columnas
    with open(profiles[0], 'r') as f:
        first = json.load(f)

    columns = ["instrument", "genre", "num_samples"]
    for feat_name in SCALAR_FEATURES:
        if feat_name in first["profile"]:
            columns.extend([f"{feat_name}_mean", f"{feat_name}_std"])

    with open(csv_path, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(columns)

        for pp in profiles:
            with open(pp, 'r') as pf:
                profile = json.load(pf)

            row = [
                profile["instrument"],
                profile["genre"],
                profile["num_samples_analyzed"]
            ]
            for feat_name in SCALAR_FEATURES:
                if feat_name in profile["profile"]:
                    row.append(profile["profile"][feat_name]["mean"])
                    row.append(profile["profile"][feat_name]["std"])
                elif f"{feat_name}_mean" in columns:
                    row.extend(["", ""])
            writer.writerow(row)

    print(f"CSV resumen: {csv_path}")


if __name__ == "__main__":
    main()

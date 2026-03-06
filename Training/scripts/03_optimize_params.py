"""
03_optimize_params.py — Capa 2: Optimizacion de parametros del sintetizador
Usa differential evolution para encontrar parametros que minimicen
la distancia timbrica respecto a los perfiles reales.

Uso:
    python 03_optimize_params.py
    python 03_optimize_params.py --instrument kicks --genre trap
    python 03_optimize_params.py --maxiter 100
"""

import os
import sys
import json
import argparse
import numpy as np
from pathlib import Path
from scipy.optimize import differential_evolution
import soundfile as sf

sys.path.insert(0, os.path.dirname(__file__))
from utils.audio_features import extract_all_features, SAMPLE_RATE
from utils.synth_bridge import SynthBridge


SCRIPT_DIR = Path(__file__).parent
TRAINING_DIR = SCRIPT_DIR.parent
PROFILES_DIR = TRAINING_DIR / "data" / "profiles"
OPTIMIZED_DIR = TRAINING_DIR / "data" / "optimized_params"
AUDIO_DIR = TRAINING_DIR / "data" / "optimized_audio"


# ── Definicion de parametros por instrumento ────────────────────────

# Cada entrada: (nombre_param, min, max)
PARAM_BOUNDS = {
    "kicks": [
        ("subFreq",         30.0,   70.0),
        ("clickAmount",     0.0,    1.0),
        ("bodyDecay",       0.05,   0.5),
        ("pitchDrop",       10.0,   60.0),
        ("pitchDropTime",   0.01,   0.1),
        ("driveAmount",     0.0,    0.5),
        ("subLevel",        0.3,    1.0),
        ("tailLength",      0.05,   0.6),
    ],
    "snares": [
        ("bodyFreq",        120.0,  280.0),
        ("noiseAmount",     0.2,    0.9),
        ("bodyDecay",       0.03,   0.2),
        ("noiseDecay",      0.05,   0.25),
        ("snapAmount",      0.0,    0.8),
    ],
    "hihats": [
        ("decay",           0.01,   0.3),
        ("tone",            0.0,    1.0),
    ],
    "claps": [
        ("decay",           0.05,   0.4),
        ("spread",          0.0,    1.0),
    ],
    "percs": [
        ("freq",            100.0,  2000.0),
        ("decay",           0.02,   0.3),
        ("noiseMix",        0.0,    0.8),
    ],
    "808s": [
        ("freq",            25.0,   65.0),
        ("sustain",         0.1,    1.5),
        ("distortion",      0.0,    0.8),
        ("slide",           0.0,    1.0),
    ],
    "leads": [
        ("freq",            200.0,  1000.0),
        ("detune",          0.0,    0.05),
        ("pulseWidth",      0.1,    0.9),
        ("attack",          0.001,  0.1),
        ("decay",           0.1,    1.0),
    ],
    "plucks": [
        ("freq",            150.0,  800.0),
        ("decay",           0.05,   0.5),
        ("brightness",      0.1,    0.9),
    ],
    "pads": [
        ("freq",            80.0,   500.0),
        ("attack",          0.05,   1.0),
        ("release",         0.1,    2.0),
        ("detune",          0.0,    0.03),
    ],
    "textures": [
        ("density",         0.1,    0.9),
        ("brightness",      0.1,    0.9),
        ("movement",        0.0,    0.8),
    ],
}

# Duracion de audio generado para comparacion (ms)
SYNTH_DURATION = {
    "kicks": 300, "snares": 250, "hihats": 150, "claps": 300,
    "percs": 200, "808s": 500, "leads": 400, "plucks": 300,
    "pads": 1000, "textures": 1000,
}

# Pesos para la distancia timbrica
DISTANCE_WEIGHTS = {
    "spectral_centroid":    3.0,
    "fundamental_freq":     3.0,
    "attack_time_ms":       2.5,
    "decay_time_ms":        2.0,
    "crest_factor":         2.0,
    "brightness_index":     1.5,
    "warmth_index":         1.5,
    "noise_ratio":          1.5,
    "spectral_flatness":    1.0,
    "transient_sharpness":  1.5,
    "harmonic_ratio":       1.5,
    "pitch_drop_semitones": 2.0,
}


def timbral_distance(generated_features: dict, target_profile: dict) -> float:
    """Distancia timbrica ponderada entre audio generado y perfil target."""
    distance = 0.0
    for feature, weight in DISTANCE_WEIGHTS.items():
        if feature not in generated_features or feature not in target_profile:
            continue
        generated = generated_features[feature]
        target_mean = target_profile[feature]["mean"]
        target_std = max(target_profile[feature]["std"], 1e-6)
        distance += weight * ((generated - target_mean) / target_std) ** 2

    # Distancia MFCC (euclidea)
    if "mfcc" in generated_features and "mfcc_stats" in target_profile:
        gen_mfcc = np.array(generated_features["mfcc"])
        target_mfcc = np.array([s["mean"] for s in target_profile["mfcc_stats"]])
        n = min(len(gen_mfcc), len(target_mfcc))
        mfcc_dist = np.sqrt(np.sum((gen_mfcc[:n] - target_mfcc[:n]) ** 2))
        distance += 2.0 * mfcc_dist

    return distance


def optimize_instrument_genre(instrument: str, genre: str,
                               synth: SynthBridge,
                               maxiter: int = 200,
                               popsize: int = 25) -> dict:
    """Optimiza parametros para un instrumento/genero."""
    profile_path = PROFILES_DIR / f"{instrument}_{genre}_profile.json"
    if not profile_path.exists():
        return None

    with open(profile_path, 'r') as f:
        profile_data = json.load(f)
    target_profile = profile_data["profile"]

    param_defs = PARAM_BOUNDS.get(instrument, [])
    if not param_defs:
        return None

    param_names = [p[0] for p in param_defs]
    bounds = [(p[1], p[2]) for p in param_defs]
    duration = SYNTH_DURATION.get(instrument, 300)

    # Temp file para extract_all_features (necesita path para stereo_width)
    temp_wav = OPTIMIZED_DIR / f"_temp_{instrument}_{genre}.wav"

    eval_count = [0]

    def objective(param_values):
        params = dict(zip(param_names, param_values))
        try:
            audio = synth.generate(instrument, params, duration)
            sf.write(str(temp_wav), audio, SAMPLE_RATE)
            features = extract_all_features(str(temp_wav))
            if "error" in features:
                return 1e6
            eval_count[0] += 1
            return timbral_distance(features, target_profile)
        except Exception:
            return 1e6

    print(f"  Optimizando {instrument}/{genre}...")
    print(f"    Parametros: {len(bounds)}, maxiter={maxiter}, popsize={popsize}")

    result = differential_evolution(
        objective, bounds,
        maxiter=maxiter,
        popsize=popsize,
        seed=42,
        tol=1e-4,
        disp=False,
        workers=1,  # workers=1 por seguridad con temp files
    )

    # Limpiar temp
    if temp_wav.exists():
        temp_wav.unlink()

    optimal_params = dict(zip(param_names, result.x))

    output = {
        "instrument": instrument,
        "genre": genre,
        "optimal_params": {k: round(v, 6) for k, v in optimal_params.items()},
        "final_distance": round(float(result.fun), 4),
        "evaluations": eval_count[0],
        "converged": bool(result.success),
        "message": result.message,
    }

    print(f"    -> Distancia final: {output['final_distance']:.4f}"
          f" ({eval_count[0]} evaluaciones, converged={output['converged']})")

    return output


def main():
    parser = argparse.ArgumentParser(
        description="Optimiza parametros del sintetizador contra perfiles reales")
    parser.add_argument("--instrument", type=str, default=None)
    parser.add_argument("--genre", type=str, default=None)
    parser.add_argument("--maxiter", type=int, default=200)
    parser.add_argument("--popsize", type=int, default=25)
    parser.add_argument("--export-audio", action="store_true",
                        help="Genera WAV de ejemplo con parametros optimos")
    args = parser.parse_args()

    OPTIMIZED_DIR.mkdir(parents=True, exist_ok=True)
    AUDIO_DIR.mkdir(parents=True, exist_ok=True)

    print("=" * 60)
    print("ONE-SHOT AI — Parameter Optimization (Capa 2)")
    print("=" * 60)

    # Buscar perfiles disponibles
    profiles = sorted(PROFILES_DIR.glob("*_profile.json"))
    if not profiles:
        print("\n[!] No se encontraron perfiles.")
        print("    Ejecuta primero: python 02_build_profiles.py")
        return

    synth = SynthBridge()
    results = []

    for pp in profiles:
        name = pp.stem.replace("_profile", "")
        parts = name.rsplit("_", 1)
        if len(parts) != 2:
            continue
        inst, genre = parts

        if args.instrument and inst != args.instrument:
            continue
        if args.genre and genre != args.genre:
            continue

        result = optimize_instrument_genre(
            inst, genre, synth,
            maxiter=args.maxiter,
            popsize=args.popsize
        )

        if result is None:
            continue

        # Guardar resultado
        out_path = OPTIMIZED_DIR / f"{inst}_{genre}_optimized.json"
        with open(out_path, 'w') as f:
            json.dump(result, f, indent=2)
        results.append(result)

        # Generar audio de ejemplo
        if args.export_audio:
            audio = synth.generate(inst, result["optimal_params"],
                                   SYNTH_DURATION.get(inst, 300))
            wav_path = AUDIO_DIR / f"{inst}_{genre}_optimized.wav"
            sf.write(str(wav_path), audio, SAMPLE_RATE)

    # Resumen
    print(f"\n{'=' * 60}")
    print(f"Optimizaciones completadas: {len(results)}")
    if results:
        avg_dist = np.mean([r["final_distance"] for r in results])
        print(f"Distancia media final:     {avg_dist:.4f}")
    print(f"Resultados en: {OPTIMIZED_DIR}")
    if args.export_audio:
        print(f"Audio de ejemplo en: {AUDIO_DIR}")


if __name__ == "__main__":
    main()

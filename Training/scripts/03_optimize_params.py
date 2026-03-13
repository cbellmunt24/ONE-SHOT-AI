"""
03_optimize_params.py — Capa 2: Optimizacion de parametros del sintetizador
Usa differential evolution para encontrar parametros que minimicen
la distancia timbrica respecto a los perfiles reales.

Uso:
    python 03_optimize_params.py
    python 03_optimize_params.py --instrument kicks --genre trap
    python 03_optimize_params.py --maxiter 100
    python 03_optimize_params.py --diagnostics
"""

import os
import sys
import json
import argparse
import numpy as np
from pathlib import Path
from datetime import datetime
from scipy.optimize import differential_evolution

sys.path.insert(0, os.path.dirname(__file__))
from utils.audio_features import extract_all_features, extract_features_fast, SAMPLE_RATE
from utils.synth_bridge import SynthBridge


SCRIPT_DIR = Path(__file__).parent
TRAINING_DIR = SCRIPT_DIR.parent
PROFILES_DIR = TRAINING_DIR / "data" / "profiles"
OPTIMIZED_DIR = TRAINING_DIR / "data" / "optimized_params"
AUDIO_DIR = TRAINING_DIR / "data" / "optimized_audio"
DIAGNOSTICS_DIR = TRAINING_DIR / "data" / "diagnostics"


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
        ("noiseColor",      0.0,    1.0),
        ("wireAmount",      0.0,    1.0),
    ],
    "hihats": [
        ("decay",           0.01,   0.3),
        ("tone",            0.0,    1.0),
        ("freqRange",       200.0,  800.0),
        ("metallic",        0.0,    1.0),
        ("noiseColor",      0.0,    1.0),
        ("ringAmount",      0.0,    1.0),
    ],
    "claps": [
        ("decay",           0.05,   0.4),
        ("spread",          0.0,    1.0),
        ("numLayers",       2.0,    8.0),
        ("noiseColor",      0.0,    1.0),
        ("thickness",       0.0,    1.0),
        ("transientSnap",   0.0,    1.0),
    ],
    "percs": [
        ("freq",            100.0,  2000.0),
        ("decay",           0.02,   0.3),
        ("noiseMix",        0.0,    0.8),
        ("pitchDrop",       0.0,    24.0),
        ("woodiness",       0.0,    1.0),
        ("metallic",        0.0,    1.0),
    ],
    "808s": [
        ("freq",            25.0,   65.0),
        ("sustain",         0.1,    1.5),
        ("distortion",      0.0,    0.8),
        ("slide",           0.0,    1.0),
        ("reeseMix",        0.0,    1.0),
        ("filterEnvAmt",    0.0,    1.0),
        ("punchiness",      0.0,    1.0),
    ],
    "leads": [
        ("freq",            200.0,  1000.0),
        ("detune",          0.0,    0.05),
        ("pulseWidth",      0.1,    0.9),
        ("attack",          0.001,  0.1),
        ("decay",           0.1,    1.0),
        ("brightness",      0.0,    1.0),
        ("vibratoRate",     0.5,    8.0),
    ],
    "plucks": [
        ("freq",            150.0,  800.0),
        ("decay",           0.05,   0.5),
        ("brightness",      0.1,    0.9),
        ("damping",         0.0,    1.0),
        ("bodyResonance",   0.0,    1.0),
        ("fmAmount",        0.0,    1.0),
    ],
    "pads": [
        ("freq",            80.0,   500.0),
        ("attack",          0.05,   1.0),
        ("release",         0.1,    2.0),
        ("detune",          0.0,    0.03),
        ("warmth",          0.0,    1.0),
        ("chorusAmount",    0.0,    1.0),
        ("filterSweep",     0.0,    1.0),
        ("evolutionRate",   0.0,    1.0),
    ],
    "textures": [
        ("density",         0.1,    0.9),
        ("brightness",      0.1,    0.9),
        ("movement",        0.0,    0.8),
        ("grainSize",       0.005,  0.1),
        ("noiseColor",      0.0,    1.0),
        ("spectralTilt",    -1.0,   1.0),
        ("pitchedness",     0.0,    1.0),
        ("pitch",           0.0,    1.0),
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


def _hz_to_mel(f: float) -> float:
    """Convierte frecuencia en Hz a escala Mel."""
    return 2595.0 * np.log10(1.0 + f / 700.0)


def _iso226_weight(feature_name: str, freq_value: float = None) -> float:
    """Peso simplificado basado en curvas de igual sonoridad ISO 226.

    - 1-5 kHz: peso maximo (oido mas sensible)
    - < 100 Hz: peso reducido
    - Resto: peso neutro
    """
    if freq_value is not None:
        if 1000.0 <= freq_value <= 5000.0:
            return 1.5
        elif freq_value < 100.0:
            return 0.6
        elif freq_value > 5000.0:
            return 0.8
        else:
            return 1.0
    # Features no frecuenciales: peso neutro
    return 1.0


# Features que representan frecuencias y deben convertirse a Mel
_FREQ_FEATURES = {"spectral_centroid", "fundamental_freq", "rolloff"}

# Pesos de masking temporal (envelope importance)
_TEMPORAL_WEIGHTS = {
    "attack_time_ms":      3.0,
    "decay_time_ms":       1.0,
    "transient_sharpness": 2.5,
}

# Mapping de features a sugerencias DSP para gap report
_DSP_SUGGESTIONS = {
    "spectral_centroid":    "Add ringFreq, metallicAmount",
    "warmth_index":         "Add subHarmonics, subPhase",
    "noise_ratio":          "Add noiseColor, wireResonance",
    "harmonic_ratio":       "Add numPartials, harmonicContent",
    "brightness_index":     "Add bandpassQ, formantFreq",
    "attack_time_ms":       "Add transient shaper parameters",
    "pitch_drop_semitones": "Add pitchEnvAmount, slideShape",
}

# Pesos para multi-resolution distance (ventanas temporales)
_WINDOW_WEIGHTS = {
    "transient": 0.30,
    "attack":    0.25,
    "body":      0.25,
    "sustain":   0.10,
    "tail":      0.10,
}


def _legacy_timbral_distance(generated_features: dict, target_profile: dict) -> float:
    """Distancia timbrica ponderada LEGACY entre audio generado y perfil target."""
    distance = 0.0
    for feature, weight in DISTANCE_WEIGHTS.items():
        if feature not in generated_features or feature not in target_profile:
            continue
        generated = generated_features[feature]
        target_mean = target_profile[feature]["mean"]
        target_std = target_profile[feature]["std"]

        # Skip features with NaN in target profile or generated features
        if (target_mean is None or target_std is None
                or np.isnan(target_mean) or np.isnan(target_std)
                or np.isnan(generated)):
            continue

        target_std = max(target_std, 0.1 * max(abs(target_mean), 1.0))
        distance += weight * ((generated - target_mean) / target_std) ** 2

    # Distancia MFCC (euclidea)
    if "mfcc" in generated_features and "mfcc_stats" in target_profile:
        gen_mfcc = np.array(generated_features["mfcc"])
        target_mfcc = np.array([s["mean"] for s in target_profile["mfcc_stats"]])
        n = min(len(gen_mfcc), len(target_mfcc))
        mfcc_dist = np.sqrt(np.sum((gen_mfcc[:n] - target_mfcc[:n]) ** 2))
        if not np.isnan(mfcc_dist):
            distance += 2.0 * mfcc_dist

    return float(distance) if not np.isnan(distance) else 1e6


def _perceptual_feature_distance(generated_features: dict,
                                  target_profile: dict) -> float:
    """Distancia timbrica perceptual para un conjunto de features.

    Incluye:
    - Conversion a Mel para features frecuenciales
    - Pesos ISO 226 de igual sonoridad
    - Masking temporal (attack/decay/transient)
    - Loudness weighting (noise_ratio, harmonic_ratio bajos pesan menos)
    """
    distance = 0.0

    for feature, base_weight in DISTANCE_WEIGHTS.items():
        if feature not in generated_features or feature not in target_profile:
            continue

        generated = generated_features[feature]
        target_mean = target_profile[feature]["mean"]
        target_std = target_profile[feature]["std"]

        # Skip features with NaN in target profile (e.g. HPSS failed on samples)
        if (target_mean is None or target_std is None
                or np.isnan(target_mean) or np.isnan(target_std)
                or np.isnan(generated)):
            continue

        target_std = max(target_std, 0.1 * max(abs(target_mean), 1.0))

        # --- Mel scale para features frecuenciales ---
        if feature in _FREQ_FEATURES:
            generated_cmp = _hz_to_mel(max(generated, 0.0))
            target_cmp = _hz_to_mel(max(target_mean, 0.0))
            target_std_cmp = max(
                _hz_to_mel(max(target_mean + target_std, 0.0)) - _hz_to_mel(max(target_mean, 0.0)),
                1e-6
            )
            # ISO 226 weight basado en frecuencia target
            iso_w = _iso226_weight(feature, freq_value=target_mean)
        else:
            generated_cmp = generated
            target_cmp = target_mean
            target_std_cmp = target_std
            iso_w = _iso226_weight(feature)

        # --- Temporal masking weight ---
        temporal_w = _TEMPORAL_WEIGHTS.get(feature, 1.0)

        # --- Loudness weighting ---
        loudness_w = 1.0
        if feature == "noise_ratio":
            gen_nr = generated_features.get("noise_ratio", 0.5)
            if gen_nr < 0.1:
                loudness_w = max(gen_nr / 0.1, 0.05)
        elif feature == "harmonic_ratio":
            gen_hr = generated_features.get("harmonic_ratio", 0.5)
            if gen_hr < 0.1:
                loudness_w = max(gen_hr / 0.1, 0.05)

        # --- Combinar pesos ---
        effective_weight = base_weight * iso_w * temporal_w * loudness_w

        normalized_err = ((generated_cmp - target_cmp) / target_std_cmp) ** 2
        distance += effective_weight * normalized_err

    # Distancia MFCC (euclidea)
    if "mfcc" in generated_features and "mfcc_stats" in target_profile:
        gen_mfcc = np.array(generated_features["mfcc"])
        target_mfcc = np.array([s["mean"] for s in target_profile["mfcc_stats"]])
        n = min(len(gen_mfcc), len(target_mfcc))
        mfcc_dist = np.sqrt(np.sum((gen_mfcc[:n] - target_mfcc[:n]) ** 2))
        if not np.isnan(mfcc_dist):
            distance += 2.0 * mfcc_dist

    return float(distance) if not np.isnan(distance) else 1e6


def _multi_resolution_distance(generated_features: dict,
                                target_profile: dict) -> float:
    """Distancia multi-resolucion usando windowed_features.

    Si windowed_features esta presente en ambos (generated y target), calcula
    distancia perceptual por ventana con pesos:
      transient=0.30, attack=0.25, body=0.25, sustain=0.10, tail=0.10

    Si no estan disponibles, cae a la distancia perceptual global.
    """
    gen_windows = generated_features.get("windowed_features")
    target_windows = target_profile.get("windowed_features")

    if gen_windows is None or target_windows is None:
        return _perceptual_feature_distance(generated_features, target_profile)

    total_distance = 0.0
    total_weight = 0.0

    for window_name, weight in _WINDOW_WEIGHTS.items():
        gen_win = gen_windows.get(window_name)
        tgt_win = target_windows.get(window_name)

        if gen_win is None or tgt_win is None:
            continue

        # Construir mini-profile con mean/std para esta ventana
        win_profile = {}
        for feat_key in DISTANCE_WEIGHTS:
            if feat_key in gen_win and feat_key in tgt_win:
                # target_windows almacena mean/std del perfil
                if isinstance(tgt_win[feat_key], dict):
                    win_profile[feat_key] = tgt_win[feat_key]
                else:
                    # Si es solo un valor (no dict), usar std=1 como fallback
                    win_profile[feat_key] = {
                        "mean": tgt_win[feat_key],
                        "std": 1.0
                    }

        if win_profile:
            win_dist = _perceptual_feature_distance(gen_win, win_profile)
            total_distance += weight * win_dist
            total_weight += weight

    if total_weight < 0.01:
        return _perceptual_feature_distance(generated_features, target_profile)

    # Normalizar por peso total usado
    return total_distance / total_weight


def perceptual_timbral_distance(generated_features: dict,
                                 target_profile: dict) -> float:
    """Distancia timbrica perceptual (DEFAULT).

    Intenta usar multi-resolution si windowed_features disponible,
    si no usa distancia perceptual global.
    """
    return _multi_resolution_distance(generated_features, target_profile)


# Alias: la funcion principal de distancia es la perceptual
timbral_distance = perceptual_timbral_distance


def generate_gap_report(results: list) -> dict:
    """Genera reporte diagnostico de gaps tras la optimizacion.

    Para cada instrumento/genero con final_distance > 0.6:
    - Lo marca como GAP_DETECTADO
    - Identifica las 3 features con mayor error residual
    - Sugiere parametros DSP a anadir

    Guarda en DIAGNOSTICS_DIR/gap_report.json y print a consola.
    """
    gaps = []
    all_entries = []

    for r in results:
        inst = r["instrument"]
        genre = r["genre"]
        dist = r["final_distance"]

        entry = {
            "instrument": inst,
            "genre": genre,
            "final_distance": dist,
            "status": "OK" if dist <= 0.6 else "GAP_DETECTADO",
        }

        if dist > 0.6:
            # Cargar perfil y resultado optimizado para analizar residuos
            profile_path = PROFILES_DIR / f"{inst}_{genre}_profile.json"
            optimized_path = OPTIMIZED_DIR / f"{inst}_{genre}_optimized.json"

            feature_errors = {}
            if profile_path.exists():
                with open(profile_path, 'r') as f:
                    profile_data = json.load(f)
                target = profile_data.get("profile", {})

                # Calcular error residual por feature usando los pesos
                for feat, weight in DISTANCE_WEIGHTS.items():
                    if feat in target and isinstance(target[feat], dict):
                        tgt_mean = target[feat].get("mean", 0)
                        tgt_std = max(target[feat].get("std", 1.0), 1e-6)
                        # Usamos el std como proxy del error residual esperado
                        # (a mayor std, peor ajuste probable)
                        feature_errors[feat] = weight * (tgt_std / max(abs(tgt_mean), 1e-6))

            # Top 3 features con mayor error
            top_features = sorted(feature_errors.items(),
                                   key=lambda x: x[1], reverse=True)[:3]

            entry["top_error_features"] = [
                {"feature": feat, "weighted_error": round(err, 4)}
                for feat, err in top_features
            ]

            # Sugerencias DSP
            suggestions = []
            for feat, _ in top_features:
                if feat in _DSP_SUGGESTIONS:
                    suggestions.append({
                        "trigger_feature": feat,
                        "suggestion": _DSP_SUGGESTIONS[feat]
                    })
            entry["dsp_suggestions"] = suggestions
            gaps.append(entry)

        all_entries.append(entry)

    report = {
        "generated_at": datetime.now().isoformat(),
        "total_combinations": len(results),
        "gaps_detected": len(gaps),
        "gap_threshold": 0.6,
        "entries": all_entries,
    }

    # Guardar
    DIAGNOSTICS_DIR.mkdir(parents=True, exist_ok=True)
    report_path = DIAGNOSTICS_DIR / "gap_report.json"
    with open(report_path, 'w') as f:
        json.dump(report, f, indent=2, ensure_ascii=False)

    # Print resumen
    print(f"\n{'=' * 60}")
    print(f"DIAGNOSTICS — Gap Report")
    print(f"{'=' * 60}")
    print(f"  Combinaciones analizadas: {len(results)}")
    print(f"  Gaps detectados (dist > 0.6): {len(gaps)}")

    if gaps:
        print(f"\n  {'Instrumento/Genero':<30} {'Distancia':>10}  Top features con error")
        print(f"  {'-' * 75}")
        for g in gaps:
            label = f"{g['instrument']}/{g['genre']}"
            top_feats = ", ".join(
                e["feature"] for e in g.get("top_error_features", [])
            )
            print(f"  {label:<30} {g['final_distance']:>10.4f}  {top_feats}")
            for s in g.get("dsp_suggestions", []):
                print(f"    -> {s['suggestion']}")

    print(f"\n  Reporte guardado en: {report_path}")
    return report


def optimize_instrument_genre(instrument: str, genre: str,
                               synth: SynthBridge,
                               maxiter: int = 80,
                               popsize: int = 12) -> dict:
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

    eval_count = [0]

    def objective(param_values):
        params = dict(zip(param_names, param_values))
        try:
            audio = synth.generate(instrument, params, duration)
            features = extract_features_fast(audio, SAMPLE_RATE)
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
        tol=0.01,
        disp=False,
        workers=1,
    )

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
    parser.add_argument("--maxiter", type=int, default=80)
    parser.add_argument("--popsize", type=int, default=12)
    parser.add_argument("--export-audio", action="store_true",
                        help="Genera WAV de ejemplo con parametros optimos")
    parser.add_argument("--diagnostics", action="store_true",
                        help="Genera gap report diagnostico tras la optimizacion")
    parser.add_argument("--skip-existing", action="store_true",
                        help="Salta combinaciones que ya tienen JSON optimizado")
    args = parser.parse_args()

    OPTIMIZED_DIR.mkdir(parents=True, exist_ok=True)
    AUDIO_DIR.mkdir(parents=True, exist_ok=True)
    DIAGNOSTICS_DIR.mkdir(parents=True, exist_ok=True)

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

        # Skip existing
        out_path_check = OPTIMIZED_DIR / f"{inst}_{genre}_optimized.json"
        if args.skip_existing and out_path_check.exists():
            # Cargar resultado existente para incluirlo en resumen/diagnostics
            with open(out_path_check, 'r') as f:
                cached = json.load(f)
            results.append(cached)
            print(f"  [skip] {inst}/{genre} ya optimizado (dist={cached.get('final_distance', '?')})")
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

    # Diagnostics gap report
    if args.diagnostics and results:
        generate_gap_report(results)


if __name__ == "__main__":
    main()

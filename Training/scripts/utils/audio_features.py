"""
audio_features.py — Extraccion de features de audio para ONE-SHOT AI
Todas las features definidas en PLAN_ML_TRAINING.md Capa 1.
"""

import numpy as np
import librosa
import scipy.signal as signal
from typing import Dict, Any, Optional


SAMPLE_RATE = 44100


def load_audio(path: str, sr: int = SAMPLE_RATE, mono: bool = True) -> np.ndarray:
    """Carga audio, convierte a mono y resamplea a sr."""
    y, _ = librosa.load(path, sr=sr, mono=mono)
    return y


def load_audio_stereo(path: str, sr: int = SAMPLE_RATE) -> np.ndarray:
    """Carga audio en stereo (2, N) para analisis de stereo width."""
    y, _ = librosa.load(path, sr=sr, mono=False)
    if y.ndim == 1:
        y = np.stack([y, y])
    return y


# ── Temporales ──────────────────────────────────────────────────────

def compute_attack_time_ms(y: np.ndarray, sr: int = SAMPLE_RATE) -> float:
    """Tiempo desde silencio hasta pico (ms)."""
    env = np.abs(y)
    peak_idx = np.argmax(env)
    threshold = 0.01 * env[peak_idx]
    onset_candidates = np.where(env[:peak_idx] < threshold)[0]
    onset_idx = onset_candidates[-1] if len(onset_candidates) > 0 else 0
    return (peak_idx - onset_idx) / sr * 1000.0


def compute_decay_time_ms(y: np.ndarray, sr: int = SAMPLE_RATE) -> float:
    """Tiempo desde pico hasta -20dB (ms)."""
    env = np.abs(y)
    peak_idx = np.argmax(env)
    peak_val = env[peak_idx]
    if peak_val < 1e-10:
        return 0.0
    threshold = peak_val * 0.1  # -20dB
    below = np.where(env[peak_idx:] < threshold)[0]
    if len(below) == 0:
        return (len(y) - peak_idx) / sr * 1000.0
    return below[0] / sr * 1000.0


def compute_total_duration_ms(y: np.ndarray, sr: int = SAMPLE_RATE) -> float:
    """Duracion total del sample (ms)."""
    return len(y) / sr * 1000.0


def compute_rms_envelope(y: np.ndarray, sr: int = SAMPLE_RATE,
                         frame_ms: float = 5.0) -> np.ndarray:
    """Curva RMS en ventanas de frame_ms."""
    frame_length = int(sr * frame_ms / 1000.0)
    frame_length = max(frame_length, 1)
    hop_length = frame_length
    rms = librosa.feature.rms(y=y, frame_length=frame_length,
                               hop_length=hop_length)[0]
    return rms


def compute_peak_amplitude(y: np.ndarray) -> float:
    return float(np.max(np.abs(y)))


def compute_crest_factor(y: np.ndarray) -> float:
    """Peak / RMS ratio."""
    rms = np.sqrt(np.mean(y ** 2))
    if rms < 1e-10:
        return 0.0
    return float(np.max(np.abs(y)) / rms)


def compute_transient_sharpness(y: np.ndarray, sr: int = SAMPLE_RATE) -> float:
    """Pendiente del onset (dB/ms)."""
    env = np.abs(y)
    peak_idx = np.argmax(env)
    peak_val = env[peak_idx]
    if peak_val < 1e-10 or peak_idx == 0:
        return 0.0
    threshold = 0.01 * peak_val
    onset_candidates = np.where(env[:peak_idx] < threshold)[0]
    onset_idx = onset_candidates[-1] if len(onset_candidates) > 0 else 0
    if peak_idx == onset_idx:
        return 0.0
    time_ms = (peak_idx - onset_idx) / sr * 1000.0
    db_range = 20.0 * np.log10(peak_val / max(env[onset_idx], 1e-10))
    return float(db_range / max(time_ms, 0.001))


# ── Espectrales ─────────────────────────────────────────────────────

def compute_spectral_centroid(y: np.ndarray, sr: int = SAMPLE_RATE) -> float:
    """Centro de masa frecuencial (Hz) — media temporal."""
    cent = librosa.feature.spectral_centroid(y=y, sr=sr)[0]
    return float(np.mean(cent))


def compute_spectral_rolloff(y: np.ndarray, sr: int = SAMPLE_RATE,
                              roll_percent: float = 0.85) -> float:
    """Freq donde esta el roll_percent de la energia."""
    rolloff = librosa.feature.spectral_rolloff(y=y, sr=sr,
                                                roll_percent=roll_percent)[0]
    return float(np.mean(rolloff))


def compute_spectral_bandwidth(y: np.ndarray, sr: int = SAMPLE_RATE) -> float:
    bw = librosa.feature.spectral_bandwidth(y=y, sr=sr)[0]
    return float(np.mean(bw))


def compute_spectral_flatness(y: np.ndarray) -> float:
    """0=tonal, 1=ruido."""
    flat = librosa.feature.spectral_flatness(y=y)[0]
    return float(np.mean(flat))


def compute_spectral_tilt(y: np.ndarray, sr: int = SAMPLE_RATE) -> float:
    """Pendiente del espectro (dB/octava) via regresion lineal en log-freq."""
    S = np.abs(librosa.stft(y))
    S_mean = np.mean(S, axis=1)
    freqs = librosa.fft_frequencies(sr=sr)
    mask = freqs > 20
    freqs = freqs[mask]
    S_mean = S_mean[mask]
    S_mean = np.maximum(S_mean, 1e-10)
    log_freqs = np.log2(freqs)
    log_mag = 20.0 * np.log10(S_mean)
    if len(log_freqs) < 2:
        return 0.0
    coeffs = np.polyfit(log_freqs, log_mag, 1)
    return float(coeffs[0])  # dB/octava


def compute_mfcc(y: np.ndarray, sr: int = SAMPLE_RATE,
                 n_mfcc: int = 13) -> list:
    """13 coeficientes MFCC (media temporal)."""
    mfcc = librosa.feature.mfcc(y=y, sr=sr, n_mfcc=n_mfcc)
    return np.mean(mfcc, axis=1).tolist()


def compute_fundamental_freq(y: np.ndarray, sr: int = SAMPLE_RATE) -> float:
    """Frecuencia fundamental (Hz) usando pyin."""
    f0, voiced_flag, _ = librosa.pyin(y, fmin=20, fmax=2000, sr=sr)
    voiced_f0 = f0[voiced_flag]
    if len(voiced_f0) == 0:
        S = np.abs(librosa.stft(y))
        freqs = librosa.fft_frequencies(sr=sr)
        mean_spec = np.mean(S, axis=1)
        peak_bin = np.argmax(mean_spec[1:]) + 1
        return float(freqs[peak_bin])
    return float(np.median(voiced_f0))


def compute_harmonic_ratio(y: np.ndarray) -> float:
    """Energia armonicos / energia total."""
    y_harm, y_perc = librosa.effects.hpss(y)
    e_harm = np.sum(y_harm ** 2)
    e_total = np.sum(y ** 2)
    if e_total < 1e-10:
        return 0.0
    return float(e_harm / e_total)


def compute_pitch_contour(y: np.ndarray, sr: int = SAMPLE_RATE) -> dict:
    """Evolucion del pitch: retorna pitch_drop_semitones y pitch_drop_time_ms."""
    f0, voiced_flag, _ = librosa.pyin(y, fmin=20, fmax=4000, sr=sr)
    voiced_f0 = f0[~np.isnan(f0)]
    if len(voiced_f0) < 2:
        return {"pitch_drop_semitones": 0.0, "pitch_drop_time_ms": 0.0}
    f0_max = np.max(voiced_f0[:max(1, len(voiced_f0) // 4)])
    f0_end = np.median(voiced_f0[-max(1, len(voiced_f0) // 4):])
    if f0_end < 1.0:
        f0_end = 1.0
    semitones = 12.0 * np.log2(f0_max / f0_end)
    hop_length = 512
    max_idx = np.nanargmax(f0[:len(f0) // 4 + 1]) if len(f0) > 1 else 0
    time_ms = max_idx * hop_length / sr * 1000.0
    return {
        "pitch_drop_semitones": float(semitones),
        "pitch_drop_time_ms": float(time_ms)
    }


# ── Timbricos ───────────────────────────────────────────────────────

def compute_brightness_index(y: np.ndarray, sr: int = SAMPLE_RATE) -> float:
    """Centroid normalizado respecto a fundamental."""
    centroid = compute_spectral_centroid(y, sr)
    fundamental = compute_fundamental_freq(y, sr)
    if fundamental < 1.0:
        return 0.0
    return float(centroid / fundamental)


def compute_warmth_index(y: np.ndarray, sr: int = SAMPLE_RATE) -> float:
    """Energia <500Hz / energia total."""
    S = np.abs(librosa.stft(y))
    freqs = librosa.fft_frequencies(sr=sr)
    low_mask = freqs < 500
    e_low = np.sum(S[low_mask, :] ** 2)
    e_total = np.sum(S ** 2)
    if e_total < 1e-10:
        return 0.0
    return float(e_low / e_total)


def compute_noise_ratio(y: np.ndarray) -> float:
    """Energia no-armonica / total."""
    _, y_perc = librosa.effects.hpss(y)
    e_perc = np.sum(y_perc ** 2)
    e_total = np.sum(y ** 2)
    if e_total < 1e-10:
        return 0.0
    return float(e_perc / e_total)


def compute_stereo_width(path: str, sr: int = SAMPLE_RATE) -> float:
    """Correlacion L-R (0=mono, 1=wide). Requiere path para cargar stereo."""
    y = load_audio_stereo(path, sr)
    if y.shape[0] < 2:
        return 0.0
    left, right = y[0], y[1]
    correlation = np.corrcoef(left, right)[0, 1]
    return float(1.0 - max(correlation, 0.0))


def compute_zero_crossing_rate(y: np.ndarray) -> float:
    zcr = librosa.feature.zero_crossing_rate(y)[0]
    return float(np.mean(zcr))


# ── Extraccion completa ────────────────────────────────────────────

def extract_all_features(path: str, sr: int = SAMPLE_RATE) -> Dict[str, Any]:
    """Extrae todas las features de un sample. Retorna dict completo."""
    y = load_audio(path, sr)

    if len(y) == 0 or np.max(np.abs(y)) < 1e-10:
        return {"error": "audio vacio o silencioso", "path": path}

    pitch_contour = compute_pitch_contour(y, sr)
    mfcc = compute_mfcc(y, sr)

    features = {
        # Temporales
        "attack_time_ms": compute_attack_time_ms(y, sr),
        "decay_time_ms": compute_decay_time_ms(y, sr),
        "total_duration_ms": compute_total_duration_ms(y, sr),
        "peak_amplitude": compute_peak_amplitude(y),
        "crest_factor": compute_crest_factor(y),
        "transient_sharpness": compute_transient_sharpness(y, sr),

        # Espectrales
        "spectral_centroid": compute_spectral_centroid(y, sr),
        "spectral_rolloff_85": compute_spectral_rolloff(y, sr, 0.85),
        "spectral_bandwidth": compute_spectral_bandwidth(y, sr),
        "spectral_flatness": compute_spectral_flatness(y),
        "spectral_tilt": compute_spectral_tilt(y, sr),
        "mfcc": mfcc,
        "fundamental_freq": compute_fundamental_freq(y, sr),
        "harmonic_ratio": compute_harmonic_ratio(y),
        "pitch_drop_semitones": pitch_contour["pitch_drop_semitones"],
        "pitch_drop_time_ms": pitch_contour["pitch_drop_time_ms"],

        # Timbricos
        "brightness_index": compute_brightness_index(y, sr),
        "warmth_index": compute_warmth_index(y, sr),
        "noise_ratio": compute_noise_ratio(y),
        "stereo_width": compute_stereo_width(path, sr),
        "zero_crossing_rate": compute_zero_crossing_rate(y),
    }

    # RMS envelope se guarda aparte (es un array)
    features["rms_envelope"] = compute_rms_envelope(y, sr).tolist()

    return features

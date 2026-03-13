"""
audio_features.py — Extraccion de features de audio para ONE-SHOT AI
Todas las features definidas en PLAN_ML_TRAINING.md Capa 1.
"""

import warnings
import numpy as np
import librosa
import scipy.signal as signal
from typing import Dict, Any, Optional, Tuple


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
    f0, voiced_flag, _ = librosa.pyin(y, fmin=22, fmax=2000, sr=sr)
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
    f0, voiced_flag, _ = librosa.pyin(y, fmin=22, fmax=4000, sr=sr)
    voiced_f0 = f0[~np.isnan(f0)]
    if len(voiced_f0) < 2:
        return {"pitch_drop_semitones": 0.0, "pitch_drop_time_ms": 0.0}
    f0_max = np.max(voiced_f0[:max(1, len(voiced_f0) // 4)])
    f0_end = np.median(voiced_f0[-max(1, len(voiced_f0) // 4):])
    if f0_end < 1.0:
        f0_end = 1.0
    semitones = 12.0 * np.log2(f0_max / f0_end)
    hop_length = 512
    first_quarter = f0[:len(f0) // 4 + 1]
    if np.all(np.isnan(first_quarter)):
        max_idx = 0
    else:
        max_idx = int(np.nanargmax(first_quarter))
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


# ── Pyin compartido (evita llamar 3 veces) ─────────────────────────

def _run_pyin(y: np.ndarray, sr: int = SAMPLE_RATE,
              fmin: float = 22, fmax: float = 4000
              ) -> Tuple[np.ndarray, np.ndarray]:
    """Ejecuta pyin una sola vez. Retorna (f0, voiced_flag)."""
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        f0, voiced_flag, _ = librosa.pyin(y, fmin=fmin, fmax=fmax, sr=sr)
    return f0, voiced_flag


def _fundamental_from_pyin(f0: np.ndarray, voiced_flag: np.ndarray,
                           y: np.ndarray, sr: int = SAMPLE_RATE) -> float:
    """Calcula fundamental a partir de pyin pre-computado."""
    voiced_f0 = f0[voiced_flag]
    if len(voiced_f0) == 0:
        S = np.abs(librosa.stft(y))
        freqs = librosa.fft_frequencies(sr=sr)
        mean_spec = np.mean(S, axis=1)
        peak_bin = np.argmax(mean_spec[1:]) + 1
        return float(freqs[peak_bin])
    return float(np.median(voiced_f0))


def _pitch_contour_from_pyin(f0: np.ndarray,
                             sr: int = SAMPLE_RATE) -> dict:
    """Calcula pitch contour a partir de pyin pre-computado."""
    voiced_f0 = f0[~np.isnan(f0)]
    if len(voiced_f0) < 2:
        return {"pitch_drop_semitones": 0.0, "pitch_drop_time_ms": 0.0}
    f0_max = np.max(voiced_f0[:max(1, len(voiced_f0) // 4)])
    f0_end = np.median(voiced_f0[-max(1, len(voiced_f0) // 4):])
    if f0_end < 1.0:
        f0_end = 1.0
    semitones = 12.0 * np.log2(f0_max / f0_end)
    hop_length = 512
    first_quarter = f0[:len(f0) // 4 + 1]
    if np.all(np.isnan(first_quarter)):
        max_idx = 0
    else:
        max_idx = int(np.nanargmax(first_quarter))
    time_ms = max_idx * hop_length / sr * 1000.0
    return {
        "pitch_drop_semitones": float(semitones),
        "pitch_drop_time_ms": float(time_ms)
    }


# ── Multi-resolution temporal analysis ─────────────────────────────

# Window boundaries in milliseconds
_WINDOWS = {
    "transient": (0.0, 5.0),
    "attack":    (0.0, 20.0),
    "body":      (20.0, 100.0),
    "sustain":   (100.0, 300.0),
    "tail":      (300.0, None),   # None = end of audio
}

# Minimum samples required for spectral analysis (FFT needs at least this)
_MIN_SPECTRAL_SAMPLES = 512


def _slice_window(y: np.ndarray, sr: int, start_ms: float,
                  end_ms: Optional[float]) -> Optional[np.ndarray]:
    """Extract a time window from audio. Returns None if window is empty."""
    start_sample = int(sr * start_ms / 1000.0)
    if start_sample >= len(y):
        return None
    if end_ms is None:
        end_sample = len(y)
    else:
        end_sample = min(int(sr * end_ms / 1000.0), len(y))
    if end_sample <= start_sample:
        return None
    segment = y[start_sample:end_sample]
    if len(segment) == 0 or np.max(np.abs(segment)) < 1e-10:
        return None
    return segment


def _energy_ratio(segment: np.ndarray, total_energy: float) -> float:
    """Energy in segment / total energy."""
    if total_energy < 1e-10:
        return 0.0
    return float(np.sum(segment ** 2) / total_energy)


def _peak_slope_db_ms(segment: np.ndarray, sr: int) -> float:
    """Slope from start of segment to its peak (dB/ms)."""
    env = np.abs(segment)
    peak_idx = np.argmax(env)
    peak_val = env[peak_idx]
    if peak_val < 1e-10 or peak_idx == 0:
        return 0.0
    start_val = max(env[0], 1e-10)
    db_range = 20.0 * np.log10(peak_val / start_val)
    time_ms = peak_idx / sr * 1000.0
    return float(db_range / max(time_ms, 0.001))


def _decay_rate_db_ms(segment: np.ndarray, sr: int) -> float:
    """Decay rate from peak to end of segment (dB/ms). Negative = decaying."""
    env = np.abs(segment)
    peak_idx = np.argmax(env)
    peak_val = env[peak_idx]
    if peak_val < 1e-10:
        return 0.0
    # RMS of last 10% of the segment as end level
    tail_len = max(1, len(segment) // 10)
    end_rms = np.sqrt(np.mean(segment[-tail_len:] ** 2))
    end_rms = max(end_rms, 1e-10)
    db_drop = 20.0 * np.log10(end_rms / peak_val)
    time_ms = (len(segment) - peak_idx) / sr * 1000.0
    if time_ms < 0.001:
        return 0.0
    return float(db_drop / time_ms)


def _segment_spectral_centroid(segment: np.ndarray, sr: int) -> float:
    """Spectral centroid for a segment, returns 0.0 if too short for FFT."""
    if len(segment) < _MIN_SPECTRAL_SAMPLES:
        return 0.0
    cent = librosa.feature.spectral_centroid(y=segment, sr=sr)[0]
    return float(np.mean(cent))


def _segment_spectral_tilt(segment: np.ndarray, sr: int) -> float:
    """Spectral tilt for a segment (dB/octave)."""
    if len(segment) < _MIN_SPECTRAL_SAMPLES:
        return 0.0
    S = np.abs(librosa.stft(segment))
    S_mean = np.mean(S, axis=1)
    freqs = librosa.fft_frequencies(sr=sr)
    mask = freqs > 20
    freqs = freqs[mask]
    S_mean = S_mean[mask]
    S_mean = np.maximum(S_mean, 1e-10)
    if len(freqs) < 2:
        return 0.0
    log_freqs = np.log2(freqs)
    log_mag = 20.0 * np.log10(S_mean)
    coeffs = np.polyfit(log_freqs, log_mag, 1)
    return float(coeffs[0])


def _segment_spectral_flatness(segment: np.ndarray) -> float:
    """Spectral flatness for a segment."""
    if len(segment) < _MIN_SPECTRAL_SAMPLES:
        return 0.0
    flat = librosa.feature.spectral_flatness(y=segment)[0]
    return float(np.mean(flat))


def _segment_spectral_rolloff(segment: np.ndarray, sr: int) -> float:
    """Spectral rolloff for a segment."""
    if len(segment) < _MIN_SPECTRAL_SAMPLES:
        return 0.0
    rolloff = librosa.feature.spectral_rolloff(y=segment, sr=sr,
                                                roll_percent=0.85)[0]
    return float(np.mean(rolloff))


def _segment_warmth(segment: np.ndarray, sr: int) -> float:
    """Energy below 500Hz / total energy in segment."""
    if len(segment) < _MIN_SPECTRAL_SAMPLES:
        return 0.0
    S = np.abs(librosa.stft(segment))
    freqs = librosa.fft_frequencies(sr=sr)
    low_mask = freqs < 500
    e_low = np.sum(S[low_mask, :] ** 2)
    e_total = np.sum(S ** 2)
    if e_total < 1e-10:
        return 0.0
    return float(e_low / e_total)


def _segment_harmonic_ratio(segment: np.ndarray) -> float:
    """Harmonic energy / total energy in segment."""
    if len(segment) < _MIN_SPECTRAL_SAMPLES:
        return 0.0
    y_harm, _ = librosa.effects.hpss(segment)
    e_harm = np.sum(y_harm ** 2)
    e_total = np.sum(segment ** 2)
    if e_total < 1e-10:
        return 0.0
    return float(e_harm / e_total)


def _segment_noise_ratio(segment: np.ndarray) -> float:
    """Percussive (noise) energy / total energy in segment."""
    if len(segment) < _MIN_SPECTRAL_SAMPLES:
        return 0.0
    _, y_perc = librosa.effects.hpss(segment)
    e_perc = np.sum(y_perc ** 2)
    e_total = np.sum(segment ** 2)
    if e_total < 1e-10:
        return 0.0
    return float(e_perc / e_total)


def _segment_fundamental_simple(segment: np.ndarray, sr: int) -> float:
    """Estimate fundamental via spectral peak (no pyin — too slow for windows)."""
    if len(segment) < _MIN_SPECTRAL_SAMPLES:
        return 0.0
    S = np.abs(librosa.stft(segment))
    freqs = librosa.fft_frequencies(sr=sr)
    mean_spec = np.mean(S, axis=1)
    if len(mean_spec) < 2:
        return 0.0
    peak_bin = np.argmax(mean_spec[1:]) + 1
    return float(freqs[peak_bin])


def extract_windowed_features(y: np.ndarray, sr: int = SAMPLE_RATE) -> Dict[str, Any]:
    """
    Multi-resolution temporal analysis.

    Splits the audio into 5 temporal phases (transient, attack, body, sustain,
    tail) and extracts phase-appropriate features from each.  Very short windows
    (< 512 samples) use only time-domain features.

    Returns a dict keyed by phase name with sub-dicts of features.
    """
    total_energy = float(np.sum(y ** 2))
    result = {}

    # ── Transient (0-5ms): time-domain only (too short for FFT) ──
    seg = _slice_window(y, sr, *_WINDOWS["transient"])
    if seg is not None:
        result["transient"] = {
            "peak_slope_db_ms": _peak_slope_db_ms(seg, sr),
            "crest_factor": compute_crest_factor(seg),
            "energy_ratio": _energy_ratio(seg, total_energy),
        }
    else:
        result["transient"] = {
            "peak_slope_db_ms": 0.0, "crest_factor": 0.0, "energy_ratio": 0.0,
        }

    # ── Attack (0-20ms) ──
    seg = _slice_window(y, sr, *_WINDOWS["attack"])
    if seg is not None:
        result["attack"] = {
            "spectral_centroid": _segment_spectral_centroid(seg, sr),
            "spectral_tilt": _segment_spectral_tilt(seg, sr),
            "energy_ratio": _energy_ratio(seg, total_energy),
        }
    else:
        result["attack"] = {
            "spectral_centroid": 0.0, "spectral_tilt": 0.0, "energy_ratio": 0.0,
        }

    # ── Body (20-100ms) ──
    seg = _slice_window(y, sr, *_WINDOWS["body"])
    if seg is not None:
        result["body"] = {
            "fundamental_freq": _segment_fundamental_simple(seg, sr),
            "warmth_index": _segment_warmth(seg, sr),
            "harmonic_ratio": _segment_harmonic_ratio(seg),
            "energy_ratio": _energy_ratio(seg, total_energy),
        }
    else:
        result["body"] = {
            "fundamental_freq": 0.0, "warmth_index": 0.0,
            "harmonic_ratio": 0.0, "energy_ratio": 0.0,
        }

    # ── Sustain (100-300ms) ──
    seg = _slice_window(y, sr, *_WINDOWS["sustain"])
    if seg is not None:
        result["sustain"] = {
            "spectral_flatness": _segment_spectral_flatness(seg),
            "noise_ratio": _segment_noise_ratio(seg),
            "spectral_centroid": _segment_spectral_centroid(seg, sr),
            "energy_ratio": _energy_ratio(seg, total_energy),
        }
    else:
        result["sustain"] = {
            "spectral_flatness": 0.0, "noise_ratio": 0.0,
            "spectral_centroid": 0.0, "energy_ratio": 0.0,
        }

    # ── Tail (300ms-end) ──
    seg = _slice_window(y, sr, *_WINDOWS["tail"])
    if seg is not None:
        result["tail"] = {
            "decay_rate_db_ms": _decay_rate_db_ms(seg, sr),
            "spectral_rolloff": _segment_spectral_rolloff(seg, sr),
            "energy_ratio": _energy_ratio(seg, total_energy),
        }
    else:
        result["tail"] = {
            "decay_rate_db_ms": 0.0, "spectral_rolloff": 0.0, "energy_ratio": 0.0,
        }

    return result


# ── Extraccion RAPIDA para optimizer (sin I/O, sin pyin, sin hpss) ──

def extract_features_fast(y: np.ndarray, sr: int = SAMPLE_RATE) -> Dict[str, Any]:
    """Extraccion rapida de features desde array numpy (sin disco).

    Para el loop de optimizacion: ~10-20x mas rapida que extract_all_features.
    Omite: pyin (lento), hpss (lento), stereo_width (mono), windowed_features.
    Usa aproximaciones rapidas para fundamental_freq, harmonic_ratio, noise_ratio.
    """
    if len(y) == 0 or np.max(np.abs(y)) < 1e-10:
        return {"error": "audio vacio"}

    # --- STFT una sola vez (reutilizar para todo) ---
    n_fft = min(2048, len(y))
    S = np.abs(librosa.stft(y, n_fft=n_fft))
    freqs = librosa.fft_frequencies(sr=sr, n_fft=n_fft)

    # Spectral centroid
    S_sum = np.sum(S, axis=0)
    S_sum_safe = np.where(S_sum < 1e-10, 1e-10, S_sum)
    centroid_frames = np.sum(freqs[:, None] * S, axis=0) / S_sum_safe
    centroid = float(np.mean(centroid_frames))

    # Spectral rolloff 85%
    cumsum = np.cumsum(S, axis=0)
    total_energy_frames = cumsum[-1, :]
    threshold = 0.85 * total_energy_frames
    rolloff_bins = np.argmax(cumsum >= threshold[None, :], axis=0)
    rolloff = float(np.mean(freqs[np.minimum(rolloff_bins, len(freqs) - 1)]))

    # Spectral bandwidth
    deviation = np.abs(freqs[:, None] - centroid_frames[None, :])
    bw = float(np.mean(np.sum(deviation * S, axis=0) / S_sum_safe))

    # Spectral flatness
    log_S = np.log(np.maximum(S, 1e-10))
    geo_mean = np.exp(np.mean(log_S, axis=0))
    arith_mean = np.maximum(np.mean(S, axis=0), 1e-10)
    flatness = float(np.mean(geo_mean / arith_mean))

    # Spectral tilt
    S_mean = np.mean(S, axis=1)
    mask = freqs > 20
    if np.sum(mask) >= 2:
        log_f = np.log2(freqs[mask])
        log_m = 20.0 * np.log10(np.maximum(S_mean[mask], 1e-10))
        tilt = float(np.polyfit(log_f, log_m, 1)[0])
    else:
        tilt = 0.0

    # Fundamental freq (FFT peak, no pyin)
    peak_bin = np.argmax(S_mean[1:]) + 1
    fundamental = float(freqs[peak_bin]) if peak_bin < len(freqs) else 0.0

    # Brightness index
    brightness = float(centroid / fundamental) if fundamental >= 1.0 else 0.0

    # Warmth index (energia <500Hz)
    low_mask = freqs < 500
    e_low = np.sum(S[low_mask, :] ** 2)
    e_total_S = np.sum(S ** 2)
    warmth = float(e_low / max(e_total_S, 1e-10))

    # Noise ratio aproximado (flatness como proxy, sin hpss)
    noise_ratio = min(flatness * 2.0, 1.0)

    # Harmonic ratio aproximado
    harmonic_ratio = 1.0 - noise_ratio

    # MFCC (usa S ya computado)
    mel_S = librosa.feature.melspectrogram(S=S**2, sr=sr, n_fft=n_fft)
    mfcc = librosa.feature.mfcc(S=librosa.power_to_db(mel_S), n_mfcc=13)
    mfcc_mean = np.mean(mfcc, axis=1).tolist()

    # Temporales (solo time-domain, muy rapido)
    env = np.abs(y)
    peak_idx = np.argmax(env)
    peak_val = env[peak_idx]

    # Attack time
    threshold_onset = 0.01 * peak_val
    onset_candidates = np.where(env[:max(peak_idx, 1)] < threshold_onset)[0]
    onset_idx = onset_candidates[-1] if len(onset_candidates) > 0 else 0
    attack_time = (peak_idx - onset_idx) / sr * 1000.0

    # Decay time
    threshold_decay = peak_val * 0.1
    below = np.where(env[peak_idx:] < threshold_decay)[0]
    decay_time = below[0] / sr * 1000.0 if len(below) > 0 else (len(y) - peak_idx) / sr * 1000.0

    # Transient sharpness
    if peak_idx > onset_idx and peak_val > 1e-10:
        db_range = 20.0 * np.log10(peak_val / max(env[onset_idx], 1e-10))
        time_ms = max((peak_idx - onset_idx) / sr * 1000.0, 0.001)
        transient = db_range / time_ms
    else:
        transient = 0.0

    # Crest factor
    rms = np.sqrt(np.mean(y ** 2))
    crest = float(peak_val / max(rms, 1e-10))

    # Pitch drop (aproximacion rapida sin pyin)
    n_quarters = max(len(y) // 4, n_fft)
    if len(y) > 2 * n_fft:
        S_start = np.abs(librosa.stft(y[:n_quarters], n_fft=n_fft))
        S_end = np.abs(librosa.stft(y[-n_quarters:], n_fft=n_fft))
        f_start = freqs[np.argmax(np.mean(S_start, axis=1)[1:]) + 1]
        f_end = freqs[np.argmax(np.mean(S_end, axis=1)[1:]) + 1]
        if f_end >= 1.0 and f_start >= 1.0:
            pitch_drop = 12.0 * np.log2(max(f_start, 1.0) / max(f_end, 1.0))
        else:
            pitch_drop = 0.0
    else:
        pitch_drop = 0.0

    result = {
        "attack_time_ms": float(attack_time),
        "decay_time_ms": float(decay_time),
        "total_duration_ms": len(y) / sr * 1000.0,
        "peak_amplitude": float(peak_val),
        "crest_factor": crest,
        "transient_sharpness": float(transient),
        "spectral_centroid": centroid,
        "spectral_rolloff_85": rolloff,
        "spectral_bandwidth": bw,
        "spectral_flatness": flatness,
        "spectral_tilt": tilt,
        "mfcc": mfcc_mean,
        "fundamental_freq": fundamental,
        "harmonic_ratio": harmonic_ratio,
        "pitch_drop_semitones": float(pitch_drop),
        "pitch_drop_time_ms": 0.0,
        "brightness_index": brightness,
        "warmth_index": warmth,
        "noise_ratio": noise_ratio,
        "stereo_width": 0.0,
        "zero_crossing_rate": float(np.mean(librosa.feature.zero_crossing_rate(y)[0])),
    }

    # Sanitize NaN values: replace with 0.0 to prevent NaN propagation
    for key, val in result.items():
        if isinstance(val, float) and np.isnan(val):
            result[key] = 0.0
        elif isinstance(val, list):
            result[key] = [0.0 if (isinstance(v, float) and np.isnan(v)) else v
                           for v in val]

    return result


# ── Extraccion completa ────────────────────────────────────────────

MAX_DURATION_S = 10.0  # One-shots no deberian durar mas de 10s


def extract_all_features(path: str, sr: int = SAMPLE_RATE) -> Dict[str, Any]:
    """Extrae todas las features de un sample. Retorna dict completo."""
    y = load_audio(path, sr)

    if len(y) == 0 or np.max(np.abs(y)) < 1e-10:
        return {"error": "audio vacio o silencioso", "path": path}

    duration_s = len(y) / sr
    if duration_s > MAX_DURATION_S:
        return {"error": f"sample demasiado largo ({duration_s:.1f}s > {MAX_DURATION_S}s), probablemente no es un one-shot", "path": path}

    # Pyin una sola vez (operacion mas costosa)
    f0, voiced_flag = _run_pyin(y, sr)
    fundamental = _fundamental_from_pyin(f0, voiced_flag, y, sr)
    pitch_contour = _pitch_contour_from_pyin(f0, sr)

    # Spectral centroid una sola vez (se reutiliza en brightness_index)
    centroid = compute_spectral_centroid(y, sr)
    brightness = float(centroid / fundamental) if fundamental >= 1.0 else 0.0

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
        "spectral_centroid": centroid,
        "spectral_rolloff_85": compute_spectral_rolloff(y, sr, 0.85),
        "spectral_bandwidth": compute_spectral_bandwidth(y, sr),
        "spectral_flatness": compute_spectral_flatness(y),
        "spectral_tilt": compute_spectral_tilt(y, sr),
        "mfcc": mfcc,
        "fundamental_freq": fundamental,
        "harmonic_ratio": compute_harmonic_ratio(y),
        "pitch_drop_semitones": pitch_contour["pitch_drop_semitones"],
        "pitch_drop_time_ms": pitch_contour["pitch_drop_time_ms"],

        # Timbricos
        "brightness_index": brightness,
        "warmth_index": compute_warmth_index(y, sr),
        "noise_ratio": compute_noise_ratio(y),
        "stereo_width": compute_stereo_width(path, sr),
        "zero_crossing_rate": compute_zero_crossing_rate(y),
    }

    # RMS envelope se guarda aparte (es un array)
    features["rms_envelope"] = compute_rms_envelope(y, sr).tolist()

    # Multi-resolution temporal analysis
    features["windowed_features"] = extract_windowed_features(y, sr)

    return features

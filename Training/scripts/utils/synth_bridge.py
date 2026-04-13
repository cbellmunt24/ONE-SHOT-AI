"""
synth_bridge.py — Universal one-shot synthesizer (Python mirror of C++ UniversalSynth).

Single generate() function takes 88 params matching UniversalSynthParams and renders audio.
Signal flow mirrors the C++ implementation:
  1. Amplitude envelope  2. Pitch envelope  3. Tonal layer  4. Noise layer
  5. Modal/KS layer  6. Transient layer  7. Mix  8. Envelope  9. Filter
  10. Effects  11. DC block + normalize
"""

import numpy as np
from typing import Dict, Optional, List, Tuple

# ---------------------------------------------------------------------------
# Parameter specification
# ---------------------------------------------------------------------------

PARAM_NAMES = [
    "oscType", "basePitch", "duration", "masterGain", "stereoWidth", "pan",
    "pitchEnvDepth", "pitchEnvFast", "pitchEnvSlow", "pitchEnvBalance",
    "pitchHoldTime", "pitchBounce", "pitchWobble", "wobbleRate",
    "tonalLevel", "bodyHarmonics", "fmDepth", "fmRatio", "fmDecay",
    "additiveAmt", "harmonic2", "harmonic3", "harmonic4", "harmonic5",
    "inharmonicity", "unisonVoices", "unisonDetune", "unisonDrift",
    "phaseDistort", "phaseDistDecay", "subLevel", "subDetune",
    "noiseLevel", "noiseColor", "noiseFilterFreq", "noiseFilterQ",
    "noiseDecay", "burstCount", "burstSpacing", "noiseAttack",
    "residualAmt", "residualLevel", "noiseHP", "noiseStereo",
    "noiseEvolution", "granularDensity",
    "modalLevel", "modalMode", "numModes", "modeDecay", "modeSpread",
    "modeRatioBase", "modeDamping", "ksFeedback", "ksDamping",
    "ksBrightness", "ksPickPosition", "ksBodyResonance",
    "transientLevel", "clickType", "clickFreq", "clickDecay",
    "clickWidth", "snapAmount", "transientSampleAmt", "topNoise",
    "ampAttack", "ampPunchDecay", "ampBodyDecay", "ampPunchLevel",
    "envSustainLevel", "envSustainTime", "envRelease", "envCurve",
    "filterCutoff", "filterReso", "filterSweepAmt", "filterSweepStart",
    "filterSweepEnd", "formantAmt", "formantFreq1", "formantFreq2",
    "reverbAmt", "reverbDecay", "chorusAmt", "satAmount", "satType",
    "compAmount",
]

PARAM_BOUNDS: List[Tuple] = [
    ("oscType",         0, 13),
    ("basePitch",       20.0, 12000.0),
    ("duration",        0.01, 5.0),
    ("masterGain",      0.0, 1.0),
    ("stereoWidth",     0.0, 1.0),
    ("pan",             0.0, 1.0),
    ("pitchEnvDepth",   0.0, 96.0),
    ("pitchEnvFast",    0.0003, 0.05),
    ("pitchEnvSlow",    0.003, 0.5),
    ("pitchEnvBalance", 0.0, 1.0),
    ("pitchHoldTime",   0.0, 0.03),
    ("pitchBounce",     0.0, 1.0),
    ("pitchWobble",     0.0, 1.0),
    ("wobbleRate",      1.0, 40.0),
    ("tonalLevel",      0.0, 1.0),
    ("bodyHarmonics",   0.0, 1.0),
    ("fmDepth",         0.0, 1.0),
    ("fmRatio",         0.5, 12.0),
    ("fmDecay",         0.003, 0.3),
    ("additiveAmt",     0.0, 1.0),
    ("harmonic2",       0.0, 1.0),
    ("harmonic3",       0.0, 1.0),
    ("harmonic4",       0.0, 1.0),
    ("harmonic5",       0.0, 1.0),
    ("inharmonicity",   0.0, 0.1),
    ("unisonVoices",    0.0, 1.0),
    ("unisonDetune",    0.1, 80.0),
    ("unisonDrift",     0.0, 1.0),
    ("phaseDistort",    0.0, 1.0),
    ("phaseDistDecay",  0.003, 0.2),
    ("subLevel",        0.0, 1.0),
    ("subDetune",       -12.0, 12.0),
    ("noiseLevel",      0.0, 1.0),
    ("noiseColor",      0.0, 1.0),
    ("noiseFilterFreq", 100.0, 16000.0),
    ("noiseFilterQ",    0.0, 1.0),
    ("noiseDecay",      0.005, 2.0),
    ("burstCount",      1.0, 8.0),
    ("burstSpacing",    0.003, 0.03),
    ("noiseAttack",     0.0, 0.02),
    ("residualAmt",     0.0, 1.0),
    ("residualLevel",   0.0, 1.0),
    ("noiseHP",         20.0, 2000.0),
    ("noiseStereo",     0.0, 1.0),
    ("noiseEvolution",  0.0, 1.0),
    ("granularDensity", 0.0, 1.0),
    ("modalLevel",      0.0, 1.0),
    ("modalMode",       0.0, 1.0),
    ("numModes",        1.0, 12.0),
    ("modeDecay",       0.01, 2.0),
    ("modeSpread",      0.0, 1.0),
    ("modeRatioBase",   1.0, 4.0),
    ("modeDamping",     0.0, 1.0),
    ("ksFeedback",      0.9, 0.999),
    ("ksDamping",       0.0, 1.0),
    ("ksBrightness",    0.0, 1.0),
    ("ksPickPosition",  0.0, 1.0),
    ("ksBodyResonance", 0.0, 1.0),
    ("transientLevel",  0.0, 1.0),
    ("clickType",       0, 3),
    ("clickFreq",       500.0, 14000.0),
    ("clickDecay",      0.0001, 0.015),
    ("clickWidth",      0.0, 1.0),
    ("snapAmount",      0.0, 1.0),
    ("transientSampleAmt", 0.0, 1.0),
    ("topNoise",        0.0, 1.0),
    ("ampAttack",       0.0001, 0.05),
    ("ampPunchDecay",   0.005, 0.5),
    ("ampBodyDecay",    0.02, 3.0),
    ("ampPunchLevel",   0.0, 1.0),
    ("envSustainLevel", 0.0, 1.0),
    ("envSustainTime",  0.0, 3.0),
    ("envRelease",      0.005, 5.0),
    ("envCurve",        0.1, 4.0),
    ("filterCutoff",    100.0, 20000.0),
    ("filterReso",      0.0, 0.95),
    ("filterSweepAmt",  0.0, 1.0),
    ("filterSweepStart", 200.0, 18000.0),
    ("filterSweepEnd",  50.0, 5000.0),
    ("formantAmt",      0.0, 1.0),
    ("formantFreq1",    200.0, 3500.0),
    ("formantFreq2",    400.0, 5000.0),
    ("reverbAmt",       0.0, 1.0),
    ("reverbDecay",     0.03, 2.0),
    ("chorusAmt",       0.0, 1.0),
    ("satAmount",       0.0, 1.0),
    ("satType",         0, 2),
    ("compAmount",      0.0, 1.0),
]

PARAM_INDEX = {name: i for i, name in enumerate(PARAM_NAMES)}

SAMPLE_RATE = 44100

# Inharmonic mode ratios (TR-808 style)
_INHARMONIC_RATIOS = [1.0, 1.447, 1.617, 1.927, 2.503, 2.664,
                      3.156, 3.571, 4.098, 4.530, 5.012, 5.623]
_HARMONIC_RATIOS = [float(i) for i in range(1, 13)]


# ---------------------------------------------------------------------------
# DSP helpers
# ---------------------------------------------------------------------------

def _clamp(x, lo, hi):
    return max(lo, min(hi, x))


def _fast_exp(t, tau):
    """Exponential decay, safe against zero tau."""
    return np.exp(-t / max(tau, 1e-7))


def _one_pole_lp(signal, cutoff_hz, sr):
    """Apply a one-pole low-pass filter (scalar cutoff)."""
    if cutoff_hz >= sr * 0.499:
        return signal.copy()
    w = 2.0 * np.pi * cutoff_hz / sr
    alpha = w / (1.0 + w)
    out = np.empty_like(signal)
    out[0] = signal[0] * alpha
    for i in range(1, len(signal)):
        out[i] = alpha * signal[i] + (1.0 - alpha) * out[i - 1]
    return out


def _one_pole_lp_varying(signal, alpha_arr):
    """One-pole LP with per-sample alpha (pre-computed)."""
    out = np.empty_like(signal)
    out[0] = signal[0] * alpha_arr[0]
    for i in range(1, len(signal)):
        a = alpha_arr[i]
        out[i] = a * signal[i] + (1.0 - a) * out[i - 1]
    return out


def _one_pole_hp(signal, cutoff_hz, sr):
    """Simple one-pole high-pass filter."""
    if cutoff_hz <= 1.0:
        return signal.copy()
    w = 2.0 * np.pi * cutoff_hz / sr
    alpha = 1.0 / (1.0 + w)
    out = np.empty_like(signal)
    out[0] = signal[0]
    for i in range(1, len(signal)):
        out[i] = alpha * (out[i - 1] + signal[i] - signal[i - 1])
    return out


def _biquad_bp(signal, center_hz, q, sr):
    """Second-order bandpass filter (constant-Q)."""
    if center_hz <= 0 or center_hz >= sr * 0.499 or q <= 0:
        return signal.copy()
    w0 = 2.0 * np.pi * center_hz / sr
    sin_w = np.sin(w0)
    cos_w = np.cos(w0)
    alpha = sin_w / (2.0 * q)
    b0 = alpha
    b1 = 0.0
    b2 = -alpha
    a0 = 1.0 + alpha
    a1 = -2.0 * cos_w
    a2 = 1.0 - alpha
    # Normalize
    b0 /= a0; b1 /= a0; b2 /= a0
    a1 /= a0; a2 /= a0

    n = len(signal)
    out = np.zeros(n)
    x1 = x2 = y1 = y2 = 0.0
    for i in range(n):
        x0 = signal[i]
        y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        out[i] = y0
        x2 = x1; x1 = x0
        y2 = y1; y1 = y0
    return out


def _biquad_lp(signal, cutoff_hz, q, sr):
    """Second-order Butterworth-ish low-pass filter."""
    if cutoff_hz >= sr * 0.499:
        return signal.copy()
    cutoff_hz = max(cutoff_hz, 10.0)
    w0 = 2.0 * np.pi * cutoff_hz / sr
    sin_w = np.sin(w0)
    cos_w = np.cos(w0)
    alpha = sin_w / (2.0 * max(q, 0.5))
    b0 = (1.0 - cos_w) / 2.0
    b1 = 1.0 - cos_w
    b2 = (1.0 - cos_w) / 2.0
    a0 = 1.0 + alpha
    a1 = -2.0 * cos_w
    a2 = 1.0 - alpha
    b0 /= a0; b1 /= a0; b2 /= a0
    a1 /= a0; a2 /= a0

    n = len(signal)
    out = np.zeros(n)
    x1 = x2 = y1 = y2 = 0.0
    for i in range(n):
        x0 = signal[i]
        y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        out[i] = y0
        x2 = x1; x1 = x0
        y2 = y1; y1 = y0
    return out


def _biquad_lp_sweep(signal, cutoff_start, cutoff_end, q, sr):
    """LP filter with linearly sweeping cutoff (per-sample coefficients)."""
    n = len(signal)
    cutoffs = np.linspace(cutoff_start, cutoff_end, n)
    cutoffs = np.clip(cutoffs, 10.0, sr * 0.499)
    out = np.zeros(n)
    x1 = x2 = y1 = y2 = 0.0
    q = max(q, 0.5)
    for i in range(n):
        w0 = 2.0 * np.pi * cutoffs[i] / sr
        sin_w = np.sin(w0)
        cos_w = np.cos(w0)
        alpha = sin_w / (2.0 * q)
        a0 = 1.0 + alpha
        b0 = (1.0 - cos_w) / 2.0 / a0
        b1 = (1.0 - cos_w) / a0
        b2 = b0
        a1_ = -2.0 * cos_w / a0
        a2_ = (1.0 - alpha) / a0
        x0 = signal[i]
        y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1_ * y1 - a2_ * y2
        out[i] = y0
        x2 = x1; x1 = x0
        y2 = y1; y1 = y0
    return out


def _dc_block(signal, coeff=0.995):
    """DC blocking filter."""
    out = np.empty_like(signal)
    xprev = 0.0
    yprev = 0.0
    for i in range(len(signal)):
        x = signal[i]
        y = x - xprev + coeff * yprev
        out[i] = y
        xprev = x
        yprev = y
    return out


def _normalize(signal, peak=0.95):
    """Normalize to target peak amplitude."""
    mx = np.max(np.abs(signal))
    if mx < 1e-10:
        return signal
    return signal * (peak / mx)


# ---------------------------------------------------------------------------
# Oscillator waveforms
# ---------------------------------------------------------------------------

def _oscillator(phase, osc_type):
    """Generate waveform from phase array (0..2pi wrapping).

    osc_type: int 0-13 matching C++ UniversalSynth.
    """
    osc_type = int(round(osc_type))
    p = phase % (2.0 * np.pi)  # wrap to [0, 2pi)
    norm_p = p / (2.0 * np.pi)  # [0, 1)

    if osc_type == 0:    # sine
        return np.sin(phase)
    elif osc_type == 1:  # triangle
        return 2.0 * np.abs(2.0 * norm_p - 1.0) - 1.0
    elif osc_type == 2:  # sawtooth
        return 2.0 * norm_p - 1.0
    elif osc_type == 3:  # square
        return np.where(norm_p < 0.5, 1.0, -1.0)
    elif osc_type == 4:  # pulse 25%
        return np.where(norm_p < 0.25, 1.0, -1.0)
    elif osc_type == 5:  # pulse 12.5%
        return np.where(norm_p < 0.125, 1.0, -1.0)
    elif osc_type == 6:  # half-rectified sine
        s = np.sin(phase)
        return np.maximum(s, 0.0) * 2.0 - 1.0
    elif osc_type == 7:  # absolute sine
        return np.abs(np.sin(phase)) * 2.0 - 1.0
    elif osc_type == 8:  # parabolic
        x = 2.0 * norm_p - 1.0  # [-1, 1)
        return 1.0 - 2.0 * x * x
    elif osc_type == 9:  # staircase (4-step)
        return np.floor(norm_p * 4.0) / 2.0 - 0.75
    elif osc_type == 10: # double sine (octave up)
        return np.sin(2.0 * phase)
    elif osc_type == 11: # clipped sine
        return np.clip(np.sin(phase) * 2.0, -1.0, 1.0)
    elif osc_type == 12: # wavetable (approximate with shaped sine in Python)
        # No wavetable file in Python — use waveshaped sine as stand-in
        s = np.sin(phase)
        return s * (1.0 - 0.3 * s * s)
    elif osc_type == 13: # noise
        return np.random.randn(len(phase))
    else:
        return np.sin(phase)


# ---------------------------------------------------------------------------
# SynthBridge class
# ---------------------------------------------------------------------------

class SynthBridge:
    """Universal one-shot synthesizer — Python mirror of C++ UniversalSynth."""

    SAMPLE_RATE = SAMPLE_RATE
    NUM_PARAMS = 88
    PARAM_BOUNDS = PARAM_BOUNDS
    PARAM_NAMES = PARAM_NAMES
    PARAM_INDEX = PARAM_INDEX

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    @staticmethod
    def generate(params_dict: Dict[str, float],
                 duration_ms: Optional[float] = None) -> np.ndarray:
        """Generate a one-shot from a parameter dictionary.

        params_dict : dict mapping param names to values
        duration_ms : override duration in ms (if None, uses params_dict['duration'] in seconds)
        Returns     : numpy array (mono, float32, normalized to ~0.95)
        """
        p = SynthBridge._resolve_params(params_dict)
        sr = SAMPLE_RATE

        # Duration
        if duration_ms is not None:
            dur_s = duration_ms / 1000.0
        else:
            dur_s = p["duration"]
        dur_s = _clamp(dur_s, 0.01, 10.0)
        n = int(sr * dur_s)
        if n < 2:
            return np.zeros(2, dtype=np.float32)
        t = np.arange(n, dtype=np.float64) / sr

        # --- 1. Amplitude envelope ---
        amp_env = SynthBridge._build_amp_envelope(p, t, n, sr)

        # --- 2. Pitch envelope ---
        pitch_env = SynthBridge._build_pitch_envelope(p, t, n, sr)
        freq = p["basePitch"] * pitch_env  # instantaneous frequency array

        # --- 3. Layer A: Tonal ---
        tonal = SynthBridge._layer_tonal(p, freq, t, n, sr)

        # --- 4. Layer B: Noise ---
        noise = SynthBridge._layer_noise(p, t, n, sr)

        # --- 5. Layer C: Modal / Karplus-Strong ---
        modal = SynthBridge._layer_modal_ks(p, t, n, sr)

        # --- 6. Layer D: Transient ---
        transient = SynthBridge._layer_transient(p, t, n, sr)

        # --- 7. Mix layers ---
        mix = (p["tonalLevel"] * tonal
               + p["noiseLevel"] * noise
               + p["modalLevel"] * modal
               + p["transientLevel"] * transient)

        # --- 8. Apply amplitude envelope ---
        mix = mix * amp_env

        # --- 9. Filter chain ---
        mix = SynthBridge._apply_filters(p, mix, t, n, sr)

        # --- 10. Effects ---
        mix = SynthBridge._apply_effects(p, mix, t, n, sr)

        # --- 11. Master gain, DC block, normalize ---
        mix = mix * p["masterGain"]
        mix = _dc_block(mix)
        mix = _normalize(mix, 0.95)

        return mix.astype(np.float32)

    @staticmethod
    def generate_from_array(params_array) -> np.ndarray:
        """Generate from a flat numpy array / list of 88 values."""
        if len(params_array) != 88:
            raise ValueError(f"Expected 88 params, got {len(params_array)}")
        d = {PARAM_NAMES[i]: float(params_array[i]) for i in range(88)}
        return SynthBridge.generate(d)

    @staticmethod
    def generate_legacy(instrument: str, params_dict: Dict[str, float],
                        duration_ms: float = 500.0) -> np.ndarray:
        """Backward-compatible interface for old pipeline code.

        Maps old instrument-specific params to universal 88-param format,
        then calls generate().
        """
        uni = SynthBridge._default_params()
        uni["duration"] = duration_ms / 1000.0

        if instrument == "kicks":
            uni["basePitch"] = params_dict.get("subFreq", 50.0)
            uni["pitchEnvDepth"] = params_dict.get("pitchDrop", 40.0)
            uni["pitchEnvFast"] = params_dict.get("pitchDropTime", 0.05)
            uni["ampBodyDecay"] = params_dict.get("bodyDecay", 0.2)
            uni["subLevel"] = params_dict.get("subLevel", 0.7)
            uni["transientLevel"] = params_dict.get("clickAmount", 0.3)
            uni["clickType"] = 0
            uni["clickDecay"] = 0.002
            uni["satAmount"] = params_dict.get("driveAmount", 0.0)
            uni["envRelease"] = params_dict.get("tailLength", 0.3)
            uni["tonalLevel"] = 0.8
            uni["noiseLevel"] = 0.0
            uni["oscType"] = 0

        elif instrument == "snares":
            uni["basePitch"] = params_dict.get("bodyFreq", 180.0)
            uni["noiseLevel"] = params_dict.get("noiseAmount", 0.6)
            uni["tonalLevel"] = 1.0 - params_dict.get("noiseAmount", 0.6)
            uni["ampBodyDecay"] = params_dict.get("bodyDecay", 0.08)
            uni["noiseDecay"] = params_dict.get("noiseDecay", 0.12)
            uni["snapAmount"] = params_dict.get("snapAmount", 0.4)
            uni["noiseColor"] = params_dict.get("noiseColor", 0.5)
            uni["transientLevel"] = params_dict.get("snapAmount", 0.4) * 0.5
            uni["oscType"] = 0

        elif instrument == "hihats":
            uni["basePitch"] = params_dict.get("freqRange", 400.0)
            uni["ampBodyDecay"] = params_dict.get("decay", 0.05)
            uni["noiseLevel"] = 0.5
            uni["noiseColor"] = params_dict.get("noiseColor", 0.5)
            uni["modalLevel"] = params_dict.get("metallic", 0.5)
            uni["modalMode"] = 0.0
            uni["modeSpread"] = 1.0
            uni["tonalLevel"] = 0.0
            uni["transientLevel"] = 0.1

        elif instrument == "claps":
            uni["noiseLevel"] = 0.8
            uni["tonalLevel"] = 0.0
            uni["noiseDecay"] = params_dict.get("decay", 0.15)
            uni["noiseColor"] = params_dict.get("noiseColor", 0.5)
            uni["burstCount"] = params_dict.get("numLayers", 4)
            uni["burstSpacing"] = 0.008 * (1.0 + params_dict.get("spread", 0.3))
            uni["transientLevel"] = params_dict.get("transientSnap", 0.5)
            uni["clickDecay"] = 0.0002

        elif instrument == "percs":
            uni["basePitch"] = params_dict.get("freq", 400.0)
            uni["ampBodyDecay"] = params_dict.get("decay", 0.06)
            uni["noiseLevel"] = params_dict.get("noiseMix", 0.3)
            uni["tonalLevel"] = 1.0 - params_dict.get("noiseMix", 0.3)
            uni["pitchEnvDepth"] = params_dict.get("pitchDrop", 0.0)
            uni["modalLevel"] = params_dict.get("metallic", 0.0) * 0.5
            uni["oscType"] = 0

        elif instrument == "808s":
            uni["basePitch"] = params_dict.get("freq", 40.0)
            uni["ampBodyDecay"] = params_dict.get("sustain", 0.5)
            uni["satAmount"] = params_dict.get("distortion", 0.3)
            uni["tonalLevel"] = 1.0
            uni["noiseLevel"] = 0.0
            uni["subLevel"] = 0.5
            uni["oscType"] = 0
            uni["pitchEnvDepth"] = params_dict.get("slide", 0.0) * 5.0
            uni["pitchEnvFast"] = 0.05
            uni["ampPunchLevel"] = params_dict.get("punchiness", 0.0)
            uni["filterSweepAmt"] = params_dict.get("filterEnvAmt", 0.0)

        elif instrument == "leads":
            uni["basePitch"] = params_dict.get("freq", 440.0)
            uni["unisonDetune"] = params_dict.get("detune", 0.02) * 1200.0
            uni["unisonVoices"] = 0.3
            uni["ampAttack"] = params_dict.get("attack", 0.01)
            uni["ampBodyDecay"] = params_dict.get("decay", 0.3)
            uni["tonalLevel"] = 1.0
            uni["noiseLevel"] = 0.0
            uni["oscType"] = 0
            brt = params_dict.get("brightness", 0.5)
            uni["filterCutoff"] = 500.0 + brt * 15000.0

        elif instrument == "plucks":
            uni["basePitch"] = params_dict.get("freq", 300.0)
            uni["ampBodyDecay"] = params_dict.get("decay", 0.2)
            uni["ksFeedback"] = 0.95
            uni["ksDamping"] = params_dict.get("damping", 0.3)
            uni["ksBrightness"] = params_dict.get("brightness", 0.5)
            uni["ksBodyResonance"] = params_dict.get("bodyResonance", 0.0)
            uni["modalLevel"] = 0.6
            uni["modalMode"] = 1.0  # KS mode
            uni["tonalLevel"] = 0.3
            uni["fmDepth"] = params_dict.get("fmAmount", 0.0)
            uni["oscType"] = 0

        elif instrument == "pads":
            uni["basePitch"] = params_dict.get("freq", 220.0)
            uni["ampAttack"] = params_dict.get("attack", 0.3)
            uni["envRelease"] = params_dict.get("release", 0.5)
            uni["unisonDetune"] = params_dict.get("detune", 0.01) * 1200.0
            uni["unisonVoices"] = 0.8
            uni["tonalLevel"] = 1.0
            uni["noiseLevel"] = 0.05
            uni["oscType"] = 2  # saw
            warmth = params_dict.get("warmth", 0.5)
            uni["filterCutoff"] = 500.0 + (1.0 - warmth) * 4000.0
            uni["chorusAmt"] = params_dict.get("chorusAmount", 0.0)
            uni["noiseEvolution"] = params_dict.get("evolutionRate", 0.0)

        elif instrument == "textures":
            uni["basePitch"] = 80.0 + params_dict.get("pitch", 0.5) * 720.0
            uni["noiseLevel"] = 0.7
            uni["tonalLevel"] = params_dict.get("pitchedness", 0.0) * 0.5
            uni["granularDensity"] = params_dict.get("density", 0.5)
            uni["noiseColor"] = params_dict.get("noiseColor", 0.5)
            uni["noiseEvolution"] = params_dict.get("movement", 0.3)
            brt = params_dict.get("brightness", 0.5)
            uni["filterCutoff"] = 500.0 + brt * 15000.0
            uni["ampAttack"] = 0.1
            uni["ampBodyDecay"] = duration_ms / 2000.0

        else:
            raise ValueError(f"Unknown legacy instrument: {instrument}")

        return SynthBridge.generate(uni, duration_ms=duration_ms)

    # ------------------------------------------------------------------
    # Default / utility
    # ------------------------------------------------------------------

    @staticmethod
    def _default_params() -> Dict[str, float]:
        """Return a dict with all 88 params set to sensible defaults."""
        d = {}
        for name, lo, hi in PARAM_BOUNDS:
            # Midpoint for most; special-case a few
            d[name] = (lo + hi) / 2.0
        # Override key defaults for a usable sound
        d["oscType"] = 0
        d["basePitch"] = 200.0
        d["duration"] = 0.5
        d["masterGain"] = 0.9
        d["stereoWidth"] = 0.0
        d["pan"] = 0.5
        d["pitchEnvDepth"] = 0.0
        d["pitchEnvBalance"] = 0.5
        d["pitchHoldTime"] = 0.0
        d["pitchBounce"] = 0.0
        d["pitchWobble"] = 0.0
        d["tonalLevel"] = 0.8
        d["bodyHarmonics"] = 0.0
        d["fmDepth"] = 0.0
        d["additiveAmt"] = 0.0
        d["unisonVoices"] = 0.0
        d["phaseDistort"] = 0.0
        d["subLevel"] = 0.0
        d["noiseLevel"] = 0.0
        d["modalLevel"] = 0.0
        d["transientLevel"] = 0.0
        d["residualAmt"] = 0.0
        d["granularDensity"] = 0.0
        d["ampAttack"] = 0.001
        d["ampPunchDecay"] = 0.02
        d["ampBodyDecay"] = 0.3
        d["ampPunchLevel"] = 0.0
        d["envSustainLevel"] = 0.0
        d["envSustainTime"] = 0.0
        d["envRelease"] = 0.1
        d["envCurve"] = 1.0
        d["filterCutoff"] = 20000.0
        d["filterReso"] = 0.0
        d["filterSweepAmt"] = 0.0
        d["formantAmt"] = 0.0
        d["reverbAmt"] = 0.0
        d["chorusAmt"] = 0.0
        d["satAmount"] = 0.0
        d["satType"] = 0
        d["compAmount"] = 0.0
        d["clickType"] = 0
        d["snapAmount"] = 0.0
        d["topNoise"] = 0.0
        d["burstCount"] = 1.0
        d["noiseAttack"] = 0.0
        d["noiseStereo"] = 0.0
        d["noiseEvolution"] = 0.0
        return d

    @staticmethod
    def _resolve_params(params_dict: Dict[str, float]) -> Dict[str, float]:
        """Fill missing params with defaults, clamp to bounds."""
        defaults = SynthBridge._default_params()
        defaults.update(params_dict)
        # Clamp
        for name, lo, hi in PARAM_BOUNDS:
            defaults[name] = _clamp(float(defaults.get(name, (lo + hi) / 2.0)), float(lo), float(hi))
        return defaults

    @staticmethod
    def param_names() -> List[str]:
        return list(PARAM_NAMES)

    @staticmethod
    def param_bounds_dict() -> Dict[str, Tuple[float, float]]:
        return {name: (lo, hi) for name, lo, hi in PARAM_BOUNDS}

    # ------------------------------------------------------------------
    # 1. Amplitude envelope
    # ------------------------------------------------------------------

    @staticmethod
    def _build_amp_envelope(p, t, n, sr):
        """Multi-stage envelope: attack -> punch decay -> body decay -> sustain -> release."""
        attack = p["ampAttack"]
        punch_decay = p["ampPunchDecay"]
        body_decay = p["ampBodyDecay"]
        punch_level = p["ampPunchLevel"]
        sustain_level = p["envSustainLevel"]
        sustain_time = p["envSustainTime"]
        release = p["envRelease"]
        curve = p["envCurve"]

        env = np.zeros(n)
        for i in range(n):
            ti = t[i]
            if ti < attack:
                # Attack phase: ramp up
                env[i] = (ti / max(attack, 1e-7)) ** curve
            elif ti < attack + punch_decay:
                # Punch decay: from 1.0 down toward body level
                dt = ti - attack
                punch_env = _clamp(1.0 - dt / max(punch_decay, 1e-7), 0.0, 1.0)
                # Punch adds extra level on top of body
                body_val = np.exp(-dt / max(body_decay, 1e-7))
                env[i] = body_val + punch_level * punch_env
            else:
                dt = ti - attack
                body_val = np.exp(-dt / max(body_decay, 1e-7))
                # Sustain floor
                env[i] = max(body_val, sustain_level) if dt < (punch_decay + sustain_time) else body_val

        # Release: apply from the end
        total_dur = t[-1] if n > 1 else 0.0
        release_start = total_dur - release
        if release_start > 0:
            for i in range(n):
                if t[i] > release_start:
                    rel_factor = np.exp(-(t[i] - release_start) / max(release * 0.5, 1e-7))
                    env[i] *= rel_factor

        return env

    # ------------------------------------------------------------------
    # 2. Pitch envelope
    # ------------------------------------------------------------------

    @staticmethod
    def _build_pitch_envelope(p, t, n, sr):
        """Pitch envelope: hold + dual-rate decay + bounce + wobble.
        Returns multiplier array (1.0 = no change)."""
        depth_st = p["pitchEnvDepth"]  # semitones
        fast = p["pitchEnvFast"]
        slow = p["pitchEnvSlow"]
        balance = p["pitchEnvBalance"]
        hold = p["pitchHoldTime"]
        bounce = p["pitchBounce"]
        wobble = p["pitchWobble"]
        wobble_rate = p["wobbleRate"]

        if depth_st < 0.01:
            return np.ones(n)

        # Time shifted by hold
        t_shifted = np.maximum(t - hold, 0.0)

        # Dual exponential decay
        env_fast = np.exp(-t_shifted / max(fast, 1e-7))
        env_slow = np.exp(-t_shifted / max(slow, 1e-7))
        env = balance * env_fast + (1.0 - balance) * env_slow

        # Bounce: damped oscillation
        if bounce > 0.005:
            bounce_sig = bounce * np.sin(2.0 * np.pi * 8.0 * t_shifted) * env_fast * 0.5
            env = env + bounce_sig

        # Wobble: LFO modulation
        if wobble > 0.005:
            wobble_sig = wobble * np.sin(2.0 * np.pi * wobble_rate * t) * 0.3
            env = env + wobble_sig * env

        # Convert semitones to frequency multiplier
        semitones = depth_st * env
        multiplier = 2.0 ** (semitones / 12.0)
        return multiplier

    # ------------------------------------------------------------------
    # 3. Layer A: Tonal
    # ------------------------------------------------------------------

    @staticmethod
    def _layer_tonal(p, freq, t, n, sr):
        """Tonal layer: oscillator + FM + additive + unison + sub + phase distortion."""
        if p["tonalLevel"] < 0.005:
            return np.zeros(n)

        osc_type = p["oscType"]

        # --- Main oscillator phase ---
        phase = 2.0 * np.pi * np.cumsum(freq) / sr

        # --- FM modulation ---
        fm_depth = p["fmDepth"]
        if fm_depth > 0.005:
            fm_ratio = p["fmRatio"]
            fm_decay = p["fmDecay"]
            mod_freq = freq * fm_ratio
            mod_phase = 2.0 * np.pi * np.cumsum(mod_freq) / sr
            fm_index = fm_depth * 8.0 * _fast_exp(t, fm_decay)
            phase = phase + fm_index * np.sin(mod_phase)

        # --- Phase distortion ---
        pd_amt = p["phaseDistort"]
        if pd_amt > 0.005:
            pd_decay = p["phaseDistDecay"]
            pd_env = pd_amt * _fast_exp(t, pd_decay)
            # Distort phase with a sine-shaped transfer
            phase = phase + pd_env * np.sin(phase * 2.0)

        # Main oscillator
        osc = _oscillator(phase, osc_type)

        # --- Body harmonics ---
        body_h = p["bodyHarmonics"]
        if body_h > 0.005:
            # Add even harmonics for body
            osc = osc + body_h * 0.5 * _oscillator(phase * 2.0, osc_type)
            osc = osc + body_h * 0.25 * _oscillator(phase * 3.0, osc_type)

        # --- Additive harmonics ---
        add_amt = p["additiveAmt"]
        if add_amt > 0.005:
            h2 = p["harmonic2"]
            h3 = p["harmonic3"]
            h4 = p["harmonic4"]
            h5 = p["harmonic5"]
            inharm = p["inharmonicity"]
            additive = np.zeros(n)
            for hi, level in [(2, h2), (3, h3), (4, h4), (5, h5)]:
                if level > 0.005:
                    # Inharmonicity stretches higher partials
                    ratio = hi * (1.0 + inharm * (hi - 1) * (hi - 1))
                    h_phase = phase * ratio
                    additive += level * np.sin(h_phase)
            osc = osc + add_amt * additive

        # --- Unison ---
        uni_voices = p["unisonVoices"]
        if uni_voices > 0.05:
            num_v = int(round(1.0 + uni_voices * 6.0))  # 1-7 voices
            if num_v > 1:
                detune_cents = p["unisonDetune"]
                drift = p["unisonDrift"]
                unison_sum = osc.copy()  # include original
                for v in range(1, num_v):
                    # Spread detuning symmetrically
                    spread = (v - (num_v - 1) / 2.0) / max((num_v - 1) / 2.0, 1.0)
                    cents = detune_cents * spread
                    # Add slow drift
                    if drift > 0.005:
                        drift_lfo = drift * 5.0 * np.sin(
                            2.0 * np.pi * (0.3 + v * 0.17) * t)
                        cents = cents + drift_lfo
                    det_ratio = 2.0 ** (cents / 1200.0)
                    v_phase = 2.0 * np.pi * np.cumsum(freq * det_ratio) / sr
                    if fm_depth > 0.005:
                        v_mod_phase = 2.0 * np.pi * np.cumsum(freq * det_ratio * p["fmRatio"]) / sr
                        v_fm = fm_depth * 8.0 * _fast_exp(t, p["fmDecay"]) * np.sin(v_mod_phase)
                        v_phase = v_phase + v_fm
                    unison_sum += _oscillator(v_phase, osc_type)
                osc = unison_sum / num_v

        # --- Sub oscillator ---
        sub_level = p["subLevel"]
        if sub_level > 0.005:
            sub_detune = p["subDetune"]
            sub_ratio = 2.0 ** (sub_detune / 12.0)
            # Sub is always sine, one octave below
            sub_phase = 2.0 * np.pi * np.cumsum(freq * 0.5 * sub_ratio) / sr
            osc = osc + sub_level * np.sin(sub_phase)

        return osc

    # ------------------------------------------------------------------
    # 4. Layer B: Noise
    # ------------------------------------------------------------------

    @staticmethod
    def _layer_noise(p, t, n, sr):
        """Noise layer: colored noise + bandpass + HP + bursts + granular."""
        if p["noiseLevel"] < 0.005:
            return np.zeros(n)

        color = p["noiseColor"]
        filter_freq = p["noiseFilterFreq"]
        filter_q = p["noiseFilterQ"]
        decay = p["noiseDecay"]
        attack = p["noiseAttack"]
        hp_freq = p["noiseHP"]
        evolution = p["noiseEvolution"]
        gran_density = p["granularDensity"]
        burst_count = p["burstCount"]
        burst_spacing = p["burstSpacing"]

        # --- Generate colored noise ---
        white = np.random.randn(n)

        # Color: 0 = dark (LP filtered), 0.5 = white, 1.0 = bright (HP-ish)
        if color < 0.45:
            # Dark noise: LP filter, lower color = lower cutoff
            lp_cut = 200.0 + color * 2.0 * 10000.0
            noise = _one_pole_lp(white, lp_cut, sr)
        elif color > 0.55:
            # Bright noise: HP filter
            hp_cut = (color - 0.5) * 2.0 * 8000.0
            noise = _one_pole_hp(white, hp_cut, sr)
        else:
            noise = white.copy()

        # --- Bandpass filter ---
        if filter_q > 0.05:
            q_val = 0.5 + filter_q * 10.0  # map 0-1 to 0.5-10.5
            noise = _biquad_bp(noise, filter_freq, q_val, sr)

        # --- High-pass ---
        if hp_freq > 25.0:
            noise = _one_pole_hp(noise, hp_freq, sr)

        # --- Amplitude envelope for noise ---
        # Attack + decay
        if attack > 0.0001:
            att_env = np.minimum(t / max(attack, 1e-7), 1.0)
        else:
            att_env = np.ones(n)
        decay_env = _fast_exp(t, decay)
        noise_env = att_env * decay_env

        # --- Burst mode ---
        if burst_count > 1.5:
            n_bursts = int(round(burst_count))
            burst_env = np.zeros(n)
            for b in range(n_bursts):
                offset_s = b * burst_spacing
                offset_n = int(offset_s * sr)
                if offset_n < n:
                    remaining = n - offset_n
                    t_burst = np.arange(remaining, dtype=np.float64) / sr
                    b_env = _fast_exp(t_burst, decay * 0.5)
                    if attack > 0.0001:
                        b_att = np.minimum(t_burst / max(attack, 1e-7), 1.0)
                        b_env = b_att * b_env
                    burst_env[offset_n:] += b_env / n_bursts
            noise_env = burst_env

        # --- Granular density modulation ---
        if gran_density > 0.05:
            # Granular gate: random amplitude modulation at grain rate
            grain_rate = 20.0 + gran_density * 200.0  # grains per second
            grain_samples = max(1, int(sr / grain_rate))
            gate = np.zeros(n)
            pos = 0
            rng = np.random.RandomState(12345)
            while pos < n:
                g_len = min(grain_samples, n - pos)
                # Hann-windowed grain
                window = 0.5 * (1.0 - np.cos(2.0 * np.pi * np.arange(g_len) / max(g_len, 1)))
                gate[pos:pos + g_len] = window * (0.5 + 0.5 * rng.rand())
                pos += grain_samples
            noise_env = noise_env * gate

        # --- Evolution: slow modulation ---
        if evolution > 0.01:
            evo_lfo = 0.5 + 0.5 * np.sin(2.0 * np.pi * 0.7 * t)
            noise_env = noise_env * (1.0 - evolution * 0.4 + evolution * 0.8 * evo_lfo)

        noise = noise * noise_env

        # Normalize noise layer amplitude
        mx = np.max(np.abs(noise))
        if mx > 1e-10:
            noise = noise / mx

        return noise

    # ------------------------------------------------------------------
    # 5. Layer C: Modal / Karplus-Strong
    # ------------------------------------------------------------------

    @staticmethod
    def _layer_modal_ks(p, t, n, sr):
        """Modal resonator bank OR Karplus-Strong string synthesis."""
        if p["modalLevel"] < 0.005:
            return np.zeros(n)

        if p["modalMode"] < 0.5:
            return SynthBridge._modal_resonators(p, t, n, sr)
        else:
            return SynthBridge._karplus_strong(p, t, n, sr)

    @staticmethod
    def _modal_resonators(p, t, n, sr):
        """Bank of bandpass filters excited by impulse + short noise burst."""
        base_freq = p["basePitch"]
        num_modes = int(round(p["numModes"]))
        num_modes = _clamp(num_modes, 1, 12)
        mode_decay = p["modeDecay"]
        spread = p["modeSpread"]
        ratio_base = p["modeRatioBase"]
        damping = p["modeDamping"]

        # Excitation: impulse + short noise burst
        excitation = np.zeros(n)
        burst_len = min(int(0.003 * sr), n)
        excitation[:burst_len] = np.random.randn(burst_len)
        excitation[:burst_len] *= np.linspace(1.0, 0.0, burst_len)
        if n > 0:
            excitation[0] += 1.0  # impulse

        output = np.zeros(n)
        for m in range(num_modes):
            if m >= len(_HARMONIC_RATIOS):
                break
            # Interpolate between harmonic and inharmonic ratios
            harm_r = _HARMONIC_RATIOS[m]
            inharm_r = _INHARMONIC_RATIOS[m] if m < len(_INHARMONIC_RATIOS) else harm_r * 1.1
            ratio = harm_r * (1.0 - spread) + inharm_r * spread
            mode_freq = base_freq * ratio_base * ratio

            if mode_freq >= sr * 0.499 or mode_freq < 20.0:
                continue

            # Per-mode decay: higher modes decay faster with damping
            mode_tau = mode_decay * (1.0 - damping * 0.7 * m / max(num_modes, 1))
            mode_tau = max(mode_tau, 0.005)

            # Q increases with decay time
            q = 5.0 + mode_tau * 30.0

            # Filter the excitation through a bandpass
            filtered = _biquad_bp(excitation, mode_freq, q, sr)

            # Apply per-mode decay envelope
            mode_env = _fast_exp(t, mode_tau)
            amplitude = 1.0 / (1.0 + m * 0.3)  # higher modes quieter
            output += filtered * mode_env * amplitude

        # Normalize
        mx = np.max(np.abs(output))
        if mx > 1e-10:
            output = output / mx
        return output

    @staticmethod
    def _karplus_strong(p, t, n, sr):
        """Karplus-Strong string synthesis with feedback delay line."""
        base_freq = p["basePitch"]
        feedback = p["ksFeedback"]
        ks_damping = p["ksDamping"]
        brightness = p["ksBrightness"]
        pick_pos = p["ksPickPosition"]
        body_res = p["ksBodyResonance"]

        delay_len = int(round(sr / max(base_freq, 20.0)))
        if delay_len < 2:
            delay_len = 2
        if delay_len >= n:
            delay_len = n - 1

        # Excitation: shaped noise burst
        excite_len = delay_len
        excitation = np.random.randn(excite_len)

        # Pick position comb filter: null at pick_pos fraction of string
        if pick_pos > 0.01 and pick_pos < 0.99:
            comb_delay = max(1, int(delay_len * pick_pos))
            for i in range(comb_delay, excite_len):
                excitation[i] -= excitation[i - comb_delay] * 0.5

        # Brightness: LP filter the excitation
        if brightness < 0.95:
            cutoff = 500.0 + brightness * 15000.0
            excitation = _one_pole_lp(excitation, cutoff, sr)

        # Delay line
        output = np.zeros(n, dtype=np.float64)
        output[:excite_len] = excitation

        # Loop filter coefficient (one-pole LP in the feedback loop)
        # Higher damping = more filtering = duller sound
        damp_coeff = 0.1 + ks_damping * 0.85  # how much LP
        prev_filtered = 0.0

        for i in range(delay_len, n):
            # Read from delay line
            delayed = output[i - delay_len]
            # One-pole averaging filter (KS core)
            avg = 0.5 * (delayed + output[i - delay_len + 1] if (i - delay_len + 1) < n else delayed)
            # Additional damping LP
            filtered = (1.0 - damp_coeff) * avg + damp_coeff * prev_filtered
            prev_filtered = filtered
            output[i] += filtered * feedback

        # Body resonance: add a resonant mode
        if body_res > 0.01:
            body_freq = base_freq * 1.5
            body_sig = _biquad_bp(output, body_freq, 3.0, sr)
            output = output + body_sig * body_res * 0.3

        # Normalize
        mx = np.max(np.abs(output))
        if mx > 1e-10:
            output = output / mx
        return output

    # ------------------------------------------------------------------
    # 6. Layer D: Transient
    # ------------------------------------------------------------------

    @staticmethod
    def _layer_transient(p, t, n, sr):
        """Transient layer: click + snap + top noise."""
        if p["transientLevel"] < 0.005:
            return np.zeros(n)

        click_type = int(round(p["clickType"]))
        click_freq = p["clickFreq"]
        click_decay = p["clickDecay"]
        click_width = p["clickWidth"]
        snap_amt = p["snapAmount"]
        top_noise_amt = p["topNoise"]

        output = np.zeros(n)

        # --- Click ---
        click_env = _fast_exp(t, click_decay)

        if click_type == 0:
            # Noise burst click
            click = np.random.randn(n) * click_env
        elif click_type == 1:
            # Impulse click (filtered)
            click = np.zeros(n)
            if n > 0:
                click[0] = 1.0
            # Width controls how many samples
            w = max(1, int(click_width * 0.001 * sr))
            if w < n:
                click[:w] = 1.0
            click = click * click_env
        elif click_type == 2:
            # FM burst click
            fm_phase = 2.0 * np.pi * np.cumsum(
                np.full(n, click_freq) + click_freq * 4.0 * click_env
            ) / sr
            click = np.sin(fm_phase) * click_env
        elif click_type == 3:
            # Chirp click (descending frequency)
            chirp_freq = click_freq * (1.0 + 3.0 * click_env)
            chirp_phase = 2.0 * np.pi * np.cumsum(chirp_freq) / sr
            click = np.sin(chirp_phase) * click_env
        else:
            click = np.random.randn(n) * click_env

        output += click

        # --- Snap ---
        if snap_amt > 0.005:
            snap_env = _fast_exp(t, 0.001)
            snap = np.random.randn(n) * snap_env * snap_amt
            # HP filter the snap for crispness
            snap = _one_pole_hp(snap, 3000.0, sr)
            output += snap

        # --- Top noise ---
        if top_noise_amt > 0.005:
            top_env = _fast_exp(t, 0.008)
            top = np.random.randn(n) * top_env * top_noise_amt
            # HP filter for airy top
            top = _one_pole_hp(top, 6000.0, sr)
            output += top

        # Normalize transient layer
        mx = np.max(np.abs(output))
        if mx > 1e-10:
            output = output / mx
        return output

    # ------------------------------------------------------------------
    # 9. Filter chain
    # ------------------------------------------------------------------

    @staticmethod
    def _apply_filters(p, signal, t, n, sr):
        """LP filter + sweep + formant."""
        cutoff = p["filterCutoff"]
        reso = p["filterReso"]
        sweep_amt = p["filterSweepAmt"]
        sweep_start = p["filterSweepStart"]
        sweep_end = p["filterSweepEnd"]
        formant_amt = p["formantAmt"]
        formant_f1 = p["formantFreq1"]
        formant_f2 = p["formantFreq2"]

        out = signal

        # --- Main LP filter ---
        if cutoff < sr * 0.48:
            q = 0.707 + reso * 10.0  # reso 0-0.95 -> Q 0.707-10.2
            out = _biquad_lp(out, cutoff, q, sr)

        # --- Filter sweep ---
        if sweep_amt > 0.01:
            q_sweep = 0.707 + reso * 5.0
            out = _biquad_lp_sweep(out, sweep_start, sweep_end, q_sweep, sr)
            # Blend swept and unsweated based on sweep_amt
            out_dry = signal if cutoff >= sr * 0.48 else _biquad_lp(signal, cutoff, 0.707 + reso * 10.0, sr)
            out = (1.0 - sweep_amt) * out_dry + sweep_amt * out

        # --- Formant filter ---
        if formant_amt > 0.01:
            # Two parallel bandpass filters to create vowel-like formants
            f1_sig = _biquad_bp(out, formant_f1, 5.0, sr)
            f2_sig = _biquad_bp(out, formant_f2, 5.0, sr)
            formant_sig = f1_sig * 0.6 + f2_sig * 0.4
            out = (1.0 - formant_amt) * out + formant_amt * formant_sig

        return out

    # ------------------------------------------------------------------
    # 10. Effects
    # ------------------------------------------------------------------

    @staticmethod
    def _apply_effects(p, signal, t, n, sr):
        """Chorus, reverb, saturation, compressor."""
        out = signal.copy()

        # --- Chorus ---
        chorus_amt = p["chorusAmt"]
        if chorus_amt > 0.005:
            delay_base = int(0.007 * sr)  # 7ms base delay
            mod_depth = int(chorus_amt * 0.003 * sr)  # up to 3ms modulation
            lfo = np.sin(2.0 * np.pi * 1.1 * t)
            chorus_sig = np.zeros(n)
            for i in range(n):
                d = delay_base + int(lfo[i] * mod_depth)
                idx = i - d
                if 0 <= idx < n:
                    chorus_sig[i] = out[idx]
            wet = chorus_amt * 0.4
            out = out * (1.0 - wet) + chorus_sig * wet

        # --- Saturation ---
        sat_amt = p["satAmount"]
        if sat_amt > 0.005:
            sat_type = int(round(p["satType"]))
            drive = 1.0 + sat_amt * 10.0
            driven = out * drive

            if sat_type == 0:
                # Soft clip (tanh)
                saturated = np.tanh(driven)
            elif sat_type == 1:
                # Tape (asinh)
                saturated = np.arcsinh(driven) / np.arcsinh(drive)
            elif sat_type == 2:
                # Asymmetric tube
                saturated = np.where(
                    driven >= 0,
                    np.tanh(driven),
                    np.tanh(driven * 0.7) * 1.2
                )
            else:
                saturated = np.tanh(driven)

            # Blend dry/wet
            out = (1.0 - sat_amt) * out + sat_amt * saturated

        # --- Reverb (simple FDN-like approximation) ---
        reverb_amt = p["reverbAmt"]
        if reverb_amt > 0.005:
            reverb_decay = p["reverbDecay"]
            # Use multiple delay lines for diffuse reverb
            delays = [int(d * sr) for d in [0.0297, 0.0371, 0.0411, 0.0437]]
            reverb_sig = np.zeros(n)
            for delay in delays:
                if delay >= n:
                    continue
                fb = 0.5 * (0.3 + reverb_decay * 0.65)
                buf = np.zeros(n)
                for i in range(delay, n):
                    buf[i] = out[i - delay] + buf[i - delay] * fb
                # LP damp the reverb tail
                buf = _one_pole_lp(buf, 4000.0, sr)
                reverb_sig += buf * 0.25
            out = out + reverb_sig * reverb_amt

        # --- Compressor (simple RMS-based) ---
        comp_amt = p["compAmount"]
        if comp_amt > 0.01:
            # RMS envelope follower
            window_size = int(0.01 * sr)  # 10ms window
            if window_size > 0:
                rms_env = np.zeros(n)
                sq = out * out
                # Cumsum-based running mean
                cs = np.cumsum(sq)
                for i in range(n):
                    start = max(0, i - window_size)
                    count = i - start + 1
                    rms_env[i] = np.sqrt((cs[i] - (cs[start - 1] if start > 0 else 0.0)) / count)

                # Threshold at -12dB, ratio based on comp_amt
                threshold = 0.25
                ratio = 1.0 + comp_amt * 7.0  # 1:1 to 1:8
                gain = np.ones(n)
                for i in range(n):
                    if rms_env[i] > threshold:
                        over = rms_env[i] / threshold
                        compressed = threshold * (over ** (1.0 / ratio))
                        gain[i] = compressed / max(rms_env[i], 1e-10)
                # Smooth gain changes
                gain = _one_pole_lp(gain, 100.0, sr)
                out = out * gain
                # Makeup gain
                out = out * (1.0 + comp_amt * 0.5)

        return out

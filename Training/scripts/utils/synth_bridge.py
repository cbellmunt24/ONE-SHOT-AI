"""
synth_bridge.py — Puente Python para generar audio con el sintetizador
Opcion B: reimplementacion basica en Python/NumPy para prototipado.
Se usara en Capa 2 para la funcion de coste de optimizacion.

NOTA: Este modulo se completara en Capa 2 cuando se necesite.
Por ahora define la interfaz y un placeholder.
"""

import numpy as np
from typing import Dict, Optional


SAMPLE_RATE = 44100


class SynthBridge:
    """
    Genera audio sintetico con parametros equivalentes al plugin C++.
    Implementacion simplificada en Python para optimizacion offline.
    """

    def __init__(self, sr: int = SAMPLE_RATE):
        self.sr = sr

    def generate(self, instrument: str, params: Dict[str, float],
                 duration_ms: float = 500.0) -> np.ndarray:
        """
        Genera audio para un instrumento con los parametros dados.
        Retorna numpy array mono.
        """
        dispatch = {
            "kicks": self._generate_kick,
            "snares": self._generate_snare,
            "hihats": self._generate_hihat,
            "claps": self._generate_clap,
            "percs": self._generate_perc,
            "808s": self._generate_808,
            "leads": self._generate_lead,
            "plucks": self._generate_pluck,
            "pads": self._generate_pad,
            "textures": self._generate_texture,
        }

        gen_func = dispatch.get(instrument)
        if gen_func is None:
            raise ValueError(f"Instrumento desconocido: {instrument}")

        return gen_func(params, duration_ms)

    # ── Generadores por instrumento ─────────────────────────────────
    # TODO: Implementar en Capa 2 replicando la sintesis del plugin C++

    def _generate_kick(self, params: Dict[str, float],
                       duration_ms: float) -> np.ndarray:
        n_samples = int(self.sr * duration_ms / 1000.0)
        t = np.arange(n_samples) / self.sr

        sub_freq = params.get("subFreq", 50.0)
        pitch_drop = params.get("pitchDrop", 40.0)
        pitch_drop_time = params.get("pitchDropTime", 0.05)
        click_amount = params.get("clickAmount", 0.3)
        body_decay = params.get("bodyDecay", 0.2)
        sub_level = params.get("subLevel", 0.7)
        drive = params.get("driveAmount", 0.0)
        tail_length = params.get("tailLength", 0.3)

        # Pitch envelope
        freq = sub_freq + pitch_drop * np.exp(-t / max(pitch_drop_time, 0.001))
        phase = 2.0 * np.pi * np.cumsum(freq) / self.sr
        body = np.sin(phase)

        # Amplitude envelope
        amp_env = np.exp(-t / max(body_decay, 0.001))
        tail_env = np.exp(-t / max(tail_length, 0.01))
        env = np.maximum(amp_env, tail_env * 0.3)

        # Click transient
        click = click_amount * np.random.randn(n_samples) * np.exp(-t / 0.002)

        # Mix
        y = (sub_level * body + click) * env

        # Simple drive
        if drive > 0:
            y = np.tanh(y * (1.0 + drive * 5.0)) / (1.0 + drive * 2.0)

        return y / max(np.max(np.abs(y)), 1e-10)

    def _generate_snare(self, params: Dict[str, float],
                        duration_ms: float) -> np.ndarray:
        n_samples = int(self.sr * duration_ms / 1000.0)
        t = np.arange(n_samples) / self.sr

        body_freq = params.get("bodyFreq", 180.0)
        noise_amount = params.get("noiseAmount", 0.6)
        body_decay = params.get("bodyDecay", 0.08)
        noise_decay = params.get("noiseDecay", 0.12)
        snap = params.get("snapAmount", 0.4)

        body = np.sin(2.0 * np.pi * body_freq * t) * np.exp(-t / body_decay)
        noise = np.random.randn(n_samples) * np.exp(-t / noise_decay)
        snap_sig = snap * np.random.randn(n_samples) * np.exp(-t / 0.003)

        y = (1.0 - noise_amount) * body + noise_amount * noise + snap_sig
        return y / max(np.max(np.abs(y)), 1e-10)

    def _generate_hihat(self, params: Dict[str, float],
                        duration_ms: float) -> np.ndarray:
        n_samples = int(self.sr * duration_ms / 1000.0)
        t = np.arange(n_samples) / self.sr

        decay = params.get("decay", 0.05)
        tone = params.get("tone", 0.5)
        n_partials = 6

        y = np.zeros(n_samples)
        base_freq = 300.0 + tone * 600.0
        for i in range(n_partials):
            freq = base_freq * (1.0 + i * 1.414)
            y += np.sin(2.0 * np.pi * freq * t) / (i + 1)

        noise = np.random.randn(n_samples) * 0.5
        y = (y + noise) * np.exp(-t / max(decay, 0.001))
        return y / max(np.max(np.abs(y)), 1e-10)

    def _generate_clap(self, params: Dict[str, float],
                       duration_ms: float) -> np.ndarray:
        n_samples = int(self.sr * duration_ms / 1000.0)
        t = np.arange(n_samples) / self.sr

        decay = params.get("decay", 0.15)
        spread = params.get("spread", 0.3)

        noise = np.random.randn(n_samples)
        env = np.exp(-t / max(decay, 0.001))

        # Multiple micro-transients
        n_hits = 3 + int(spread * 4)
        for i in range(n_hits):
            offset = int(i * self.sr * 0.008 * (1.0 + spread))
            if offset < n_samples:
                burst_len = min(int(self.sr * 0.005), n_samples - offset)
                env[offset:offset + burst_len] += 0.5

        y = noise * env
        return y / max(np.max(np.abs(y)), 1e-10)

    def _generate_perc(self, params: Dict[str, float],
                       duration_ms: float) -> np.ndarray:
        n_samples = int(self.sr * duration_ms / 1000.0)
        t = np.arange(n_samples) / self.sr

        freq = params.get("freq", 400.0)
        decay = params.get("decay", 0.06)
        noise_mix = params.get("noiseMix", 0.3)

        tone = np.sin(2.0 * np.pi * freq * t) * np.exp(-t / max(decay, 0.001))
        noise = np.random.randn(n_samples) * np.exp(-t / (decay * 0.5))
        y = (1.0 - noise_mix) * tone + noise_mix * noise
        return y / max(np.max(np.abs(y)), 1e-10)

    def _generate_808(self, params: Dict[str, float],
                      duration_ms: float) -> np.ndarray:
        n_samples = int(self.sr * duration_ms / 1000.0)
        t = np.arange(n_samples) / self.sr

        freq = params.get("freq", 40.0)
        sustain = params.get("sustain", 0.5)
        distortion = params.get("distortion", 0.3)
        slide = params.get("slide", 0.0)

        freq_env = freq + slide * 20.0 * np.exp(-t / 0.05)
        phase = 2.0 * np.pi * np.cumsum(freq_env) / self.sr
        y = np.sin(phase)

        env = np.exp(-t / max(sustain, 0.01))
        y = y * env

        if distortion > 0:
            y = np.tanh(y * (1.0 + distortion * 8.0))

        return y / max(np.max(np.abs(y)), 1e-10)

    def _generate_lead(self, params: Dict[str, float],
                       duration_ms: float) -> np.ndarray:
        n_samples = int(self.sr * duration_ms / 1000.0)
        t = np.arange(n_samples) / self.sr

        freq = params.get("freq", 440.0)
        detune = params.get("detune", 0.02)
        pulse_width = params.get("pulseWidth", 0.5)
        attack = params.get("attack", 0.01)
        decay = params.get("decay", 0.3)

        osc1 = np.sin(2.0 * np.pi * freq * t)
        osc2 = np.sin(2.0 * np.pi * freq * (1.0 + detune) * t)
        y = 0.5 * (osc1 + osc2)

        env = np.minimum(t / max(attack, 0.001), 1.0) * np.exp(
            -np.maximum(t - attack, 0) / max(decay, 0.001))
        y = y * env
        return y / max(np.max(np.abs(y)), 1e-10)

    def _generate_pluck(self, params: Dict[str, float],
                        duration_ms: float) -> np.ndarray:
        n_samples = int(self.sr * duration_ms / 1000.0)
        t = np.arange(n_samples) / self.sr

        freq = params.get("freq", 300.0)
        decay = params.get("decay", 0.2)
        brightness = params.get("brightness", 0.5)

        y = np.sin(2.0 * np.pi * freq * t)
        for h in range(2, 6):
            amp = brightness ** h
            y += amp * np.sin(2.0 * np.pi * freq * h * t)

        env = np.exp(-t / max(decay, 0.001))
        y = y * env
        return y / max(np.max(np.abs(y)), 1e-10)

    def _generate_pad(self, params: Dict[str, float],
                      duration_ms: float) -> np.ndarray:
        n_samples = int(self.sr * duration_ms / 1000.0)
        t = np.arange(n_samples) / self.sr

        freq = params.get("freq", 220.0)
        attack = params.get("attack", 0.3)
        release = params.get("release", 0.5)
        detune = params.get("detune", 0.01)
        n_voices = 4

        y = np.zeros(n_samples)
        for i in range(n_voices):
            d = 1.0 + (i - n_voices / 2) * detune / n_voices
            y += np.sin(2.0 * np.pi * freq * d * t)
        y /= n_voices

        env = np.minimum(t / max(attack, 0.001), 1.0)
        total_time = duration_ms / 1000.0
        release_start = total_time - release
        if release_start > 0:
            rel_env = np.where(t > release_start,
                               np.exp(-(t - release_start) / max(release, 0.001)),
                               1.0)
            env = env * rel_env

        y = y * env
        return y / max(np.max(np.abs(y)), 1e-10)

    def _generate_texture(self, params: Dict[str, float],
                          duration_ms: float) -> np.ndarray:
        n_samples = int(self.sr * duration_ms / 1000.0)
        t = np.arange(n_samples) / self.sr

        density = params.get("density", 0.5)
        brightness = params.get("brightness", 0.5)
        movement = params.get("movement", 0.3)

        noise = np.random.randn(n_samples)
        mod = 1.0 + movement * 0.5 * np.sin(2.0 * np.pi * 0.5 * t)

        # Simple LP filter via moving average
        kernel_size = max(2, int((1.0 - brightness) * 100))
        kernel = np.ones(kernel_size) / kernel_size
        y = np.convolve(noise, kernel, mode='same') * mod

        env = np.minimum(t / 0.1, 1.0) * np.exp(-t / max(duration_ms / 2000, 0.1))
        y = y * env
        return y / max(np.max(np.abs(y)), 1e-10)

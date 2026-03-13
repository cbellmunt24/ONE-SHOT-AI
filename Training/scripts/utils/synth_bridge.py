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
        noise_color = params.get("noiseColor", 0.5)
        wire_amount = params.get("wireAmount", 0.0)

        body = np.sin(2.0 * np.pi * body_freq * t) * np.exp(-t / body_decay)

        # Noise colored: blend white and pink (cumsum-based pink approximation)
        white = np.random.randn(n_samples)
        pink = np.cumsum(np.random.randn(n_samples))
        pink = pink - np.linspace(pink[0], pink[-1], n_samples)
        pink = pink / max(np.max(np.abs(pink)), 1e-10)
        noise = (noise_color * white + (1.0 - noise_color) * pink) * np.exp(-t / noise_decay)

        snap_sig = snap * np.random.randn(n_samples) * np.exp(-t / 0.003)

        # Wire rattle: resonant filtered noise
        wire = np.zeros(n_samples)
        if wire_amount > 0.01:
            wire_noise = np.random.randn(n_samples)
            # Simple resonant BP approximation at ~5500 Hz
            wire_freq = 5500.0 + noise_color * 2500.0
            wire_phase = 2.0 * np.pi * wire_freq * t
            wire = wire_noise * np.sin(wire_phase) * np.exp(-t / noise_decay) * wire_amount * 0.2

        y = (1.0 - noise_amount) * body + noise_amount * noise + snap_sig + wire
        return y / max(np.max(np.abs(y)), 1e-10)

    def _generate_hihat(self, params: Dict[str, float],
                        duration_ms: float) -> np.ndarray:
        n_samples = int(self.sr * duration_ms / 1000.0)
        t = np.arange(n_samples) / self.sr

        decay = params.get("decay", 0.05)
        tone = params.get("tone", 0.5)
        freq_range = params.get("freqRange", 400.0)
        metallic = params.get("metallic", 0.5)
        noise_color = params.get("noiseColor", 0.5)
        ring_amount = params.get("ringAmount", 0.0)

        # TR-808 metallic ratios (6 inharmonic partials)
        metallic_ratios = [1.0, 1.4471, 1.6170, 1.9265, 2.5028, 2.6637]
        n_partials = 6

        # Square wave partials (richer than sine, like 808 real)
        y_metal = np.zeros(n_samples)
        phases = []
        for i in range(n_partials):
            freq = freq_range * metallic_ratios[i]
            phase = 2.0 * np.pi * freq * t
            phases.append(phase)
            # Square wave approximation
            sq = np.sign(np.sin(phase))
            y_metal += sq / n_partials

        # Ring modulation between pairs (creates inharmonic sum/difference freqs)
        y_ring = np.zeros(n_samples)
        if ring_amount > 0.01:
            for j in range(0, n_partials - 1, 2):
                s1 = np.sin(phases[j])
                s2 = np.sin(phases[j + 1])
                y_ring += s1 * s2 / 3.0

        # Blend: squares + ring mod (more metallic with higher metallic param)
        metal_mix = y_metal * (1.0 - metallic * 0.4) + y_ring * metallic * 0.6

        # Noise layer: colored noise (pink vs white via noiseColor)
        white = np.random.randn(n_samples)
        pink = np.cumsum(np.random.randn(n_samples))
        pink = pink - np.linspace(pink[0], pink[-1], n_samples)
        pink = pink / max(np.max(np.abs(pink)), 1e-10)
        noise = noise_color * white + (1.0 - noise_color) * pink

        # Separate envelopes: metal decays faster, noise sustains longer
        metal_env = np.exp(-t / max(decay * 0.6, 0.0008))
        noise_env = np.exp(-t / max(decay * 1.3, 0.001))

        # Mix metal and noise layers
        y = (metal_mix * metal_env * metallic * 0.45
             + noise * noise_env * (1.0 - metallic * 0.25) * 0.5)

        # Ring resonance
        if ring_amount > 0.01:
            ring_env = np.exp(-t / max(decay * 0.45, 0.001))
            y += y_ring * ring_amount * 0.2 * ring_env

        return y / max(np.max(np.abs(y)), 1e-10)

    def _generate_clap(self, params: Dict[str, float],
                       duration_ms: float) -> np.ndarray:
        n_samples = int(self.sr * duration_ms / 1000.0)
        t = np.arange(n_samples) / self.sr

        decay = params.get("decay", 0.15)
        spread = params.get("spread", 0.3)
        num_layers = int(params.get("numLayers", 4))
        num_layers = max(2, min(8, num_layers))
        noise_color = params.get("noiseColor", 0.5)
        thickness = params.get("thickness", 0.3)
        transient_snap = params.get("transientSnap", 0.5)

        # Noise: blend white and pink via noiseColor
        white = np.random.randn(n_samples)
        pink = np.cumsum(np.random.randn(n_samples))
        pink = pink - np.linspace(pink[0], pink[-1], n_samples)
        pink = pink / max(np.max(np.abs(pink)), 1e-10)
        noise = noise_color * white + (1.0 - noise_color) * pink

        # Base decay envelope
        env = np.exp(-t / max(decay, 0.001))

        # Initial crack transient (sharper with higher transientSnap)
        crack_decay = 0.00015 + (1.0 - transient_snap) * 0.00025
        crack_env = np.exp(-t / crack_decay) * (0.5 + transient_snap * 0.5)
        crack = np.random.randn(n_samples) * crack_env

        # Layer flutter: multiple micro-transients with controlled spacing
        layer_spacing = 0.008 * (1.0 + spread)
        burst_env = np.zeros(n_samples)
        for i in range(num_layers):
            offset = int(i * self.sr * layer_spacing)
            if offset < n_samples:
                layer_decay = 0.001 + thickness * 0.002
                progress = (i + 1) / num_layers
                amplitude = 0.3 + 0.7 * progress
                layer_t = np.arange(n_samples - offset) / self.sr
                layer_env = amplitude * np.exp(-layer_t / layer_decay)
                burst_env[offset:] += layer_env

        burst_env = np.minimum(burst_env, 1.5)

        # Thickness: simple LP effect via moving average
        if thickness > 0.1:
            kernel_size = max(2, int(thickness * 20))
            kernel = np.ones(kernel_size) / kernel_size
            noise = np.convolve(noise, kernel, mode='same')

        y = crack + noise * burst_env + noise * env * 0.3
        return y / max(np.max(np.abs(y)), 1e-10)

    def _generate_perc(self, params: Dict[str, float],
                       duration_ms: float) -> np.ndarray:
        n_samples = int(self.sr * duration_ms / 1000.0)
        t = np.arange(n_samples) / self.sr

        freq = params.get("freq", 400.0)
        decay = params.get("decay", 0.06)
        noise_mix = params.get("noiseMix", 0.3)
        pitch_drop = params.get("pitchDrop", 0.0)
        woodiness = params.get("woodiness", 0.0)
        metallic = params.get("metallic", 0.0)

        # Pitch envelope: start freq drops to target freq
        start_freq = freq * (2.0 ** (pitch_drop / 12.0))
        pitch_env = np.exp(-t / max(decay * 0.2, 0.001))
        current_freq = freq + (start_freq - freq) * pitch_env

        # Main tone with pitch envelope
        phase = 2.0 * np.pi * np.cumsum(current_freq) / self.sr
        tone = np.sin(phase) * 0.8 + np.sin(phase * 2.0) * 0.2
        tone_env = np.exp(-t / max(decay, 0.001))

        # Metallic partial (inharmonic ratio like C++ harmonicRatio)
        metallic_sig = np.zeros(n_samples)
        if metallic > 0.01:
            metal_freq = current_freq * 2.7  # inharmonic ratio
            metal_phase = 2.0 * np.pi * np.cumsum(metal_freq) / self.sr
            metallic_sig = np.sin(metal_phase) * metallic * 0.4

        # Noise: colored by woodiness (more pink = more woody)
        white = np.random.randn(n_samples)
        if woodiness > 0.01:
            pink = np.cumsum(np.random.randn(n_samples))
            pink = pink - np.linspace(pink[0], pink[-1], n_samples)
            pink = pink / max(np.max(np.abs(pink)), 1e-10)
            noise = (1.0 - woodiness) * white + woodiness * pink
        else:
            noise = white
        noise_env = np.exp(-t / (decay * 0.6))

        y = ((tone + metallic_sig) * tone_env * 0.55
             + noise_mix * noise * noise_env * 0.35)
        return y / max(np.max(np.abs(y)), 1e-10)

    def _generate_808(self, params: Dict[str, float],
                      duration_ms: float) -> np.ndarray:
        n_samples = int(self.sr * duration_ms / 1000.0)
        t = np.arange(n_samples) / self.sr

        freq = params.get("freq", 40.0)
        sustain = params.get("sustain", 0.5)
        distortion = params.get("distortion", 0.3)
        slide = params.get("slide", 0.0)
        reese_mix = params.get("reeseMix", 0.0)
        filter_env_amt = params.get("filterEnvAmt", 0.0)
        punchiness = params.get("punchiness", 0.0)

        # Pitch glide
        freq_env = freq + slide * 20.0 * np.exp(-t / 0.05)
        phase = 2.0 * np.pi * np.cumsum(freq_env) / self.sr
        sine_body = np.sin(phase)

        # Reese bass layer: 3 detuned saws
        reese = np.zeros(n_samples)
        if reese_mix > 0.01:
            detune_cents = 30.0  # typical reese detune
            saw1 = 2.0 * (np.cumsum(freq_env) / self.sr % 1.0) - 1.0
            freq2 = freq_env * (2.0 ** (detune_cents / 1200.0))
            freq3 = freq_env * (2.0 ** (-detune_cents / 1200.0))
            saw2 = 2.0 * (np.cumsum(freq2) / self.sr % 1.0) - 1.0
            saw3 = 2.0 * (np.cumsum(freq3) / self.sr % 1.0) - 1.0
            reese = (saw1 * 0.45 + saw2 * 0.35 + saw3 * 0.35) / 1.15

        # Mix sine and reese
        y = sine_body * (1.0 - reese_mix) + reese * reese_mix

        # Amplitude envelope
        env = np.exp(-t / max(sustain, 0.01))
        y = y * env

        # Filter envelope (donk bass effect)
        if filter_env_amt > 0.01:
            filter_env = np.exp(-t / 0.06)
            # Approximate LP filter sweep with simple moving average that varies
            cutoff_norm = 0.3 + filter_env * filter_env_amt * 0.7
            # Simple one-pole LP approximation
            alpha = np.clip(cutoff_norm, 0.01, 0.99)
            filtered = np.zeros(n_samples)
            filtered[0] = y[0]
            for i in range(1, n_samples):
                a = alpha[i]
                filtered[i] = a * y[i] + (1.0 - a) * filtered[i - 1]
            y = filtered

        # Punch transient
        if punchiness > 0.01:
            punch_env = np.exp(-t / 0.003)
            punch = sine_body * punch_env * punchiness * 0.25
            y = y + punch

        # Distortion
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
        brightness = params.get("brightness", 0.5)
        vibrato_rate = params.get("vibratoRate", 4.0)

        # Vibrato LFO
        vibrato_depth = 0.02  # semitones base depth
        vibrato = np.sin(2.0 * np.pi * vibrato_rate * t) * vibrato_depth
        pitch_mod = 2.0 ** (vibrato / 12.0)
        mod_freq = freq * pitch_mod

        # Two detuned oscillators
        phase1 = 2.0 * np.pi * np.cumsum(mod_freq) / self.sr
        phase2 = 2.0 * np.pi * np.cumsum(mod_freq * (1.0 + detune)) / self.sr
        osc1 = np.sin(phase1)
        osc2 = np.sin(phase2)
        y = 0.5 * (osc1 + osc2)

        # Amplitude envelope
        env = np.minimum(t / max(attack, 0.001), 1.0) * np.exp(
            -np.maximum(t - attack, 0) / max(decay, 0.001))
        y = y * env

        # Brightness: simple LP filter via moving average (lower brightness = more filtering)
        if brightness < 0.95:
            kernel_size = max(2, int((1.0 - brightness) * 60))
            kernel = np.ones(kernel_size) / kernel_size
            y = np.convolve(y, kernel, mode='same')

        return y / max(np.max(np.abs(y)), 1e-10)

    def _generate_pluck(self, params: Dict[str, float],
                        duration_ms: float) -> np.ndarray:
        n_samples = int(self.sr * duration_ms / 1000.0)
        t = np.arange(n_samples) / self.sr

        freq = params.get("freq", 300.0)
        decay = params.get("decay", 0.2)
        brightness = params.get("brightness", 0.5)
        damping = params.get("damping", 0.3)
        body_resonance = params.get("bodyResonance", 0.0)
        fm_amount = params.get("fmAmount", 0.0)

        # Karplus-Strong inspired: excitation + feedback loop approximation
        y_ks = np.sin(2.0 * np.pi * freq * t)
        for h in range(2, 6):
            # Damping: higher harmonics decay faster (like C++ damping filter)
            harmonic_decay = decay * (1.0 - damping * 0.5 * (h - 1) / 5.0)
            amp = brightness ** h
            y_ks += amp * np.sin(2.0 * np.pi * freq * h * t) * np.exp(
                -t / max(harmonic_decay, 0.001)) / np.exp(-t / max(decay, 0.001) + 1e-10)

        env = np.exp(-t / max(decay, 0.001))
        y_ks = y_ks * env

        # FM synthesis layer for richer/ambient pluck tones
        if fm_amount > 0.01:
            # FM: carrier at base freq, modulator at 2x freq (harmonic ratio)
            mod_ratio = 2.0
            # FM index decays over time (bright attack, mellow tail)
            fm_index = fm_amount * 5.0 * np.exp(-t / max(decay * 0.7, 0.01))
            # Modulator
            modulator = np.sin(2.0 * np.pi * freq * mod_ratio * t)
            # Carrier with FM modulation
            carrier_phase = 2.0 * np.pi * freq * t + fm_index * modulator
            y_fm = np.sin(carrier_phase) * env

            # Add second FM pair at slightly detuned freq for richness
            mod2 = np.sin(2.0 * np.pi * freq * 1.01 * 3.0 * t)
            carrier2_phase = 2.0 * np.pi * freq * 1.005 * t + fm_index * 0.6 * mod2
            y_fm2 = np.sin(carrier2_phase) * env * 0.4

            y_fm = y_fm + y_fm2

            # Blend KS and FM based on fmAmount
            y = (1.0 - fm_amount) * y_ks + fm_amount * y_fm
        else:
            y = y_ks

        # Body resonance: resonant delay at a related frequency (like C++ bodyDelay)
        if body_resonance > 0.01:
            body_freq = freq * 1.5
            body_period = int(self.sr / body_freq)
            if body_period > 0 and body_period < n_samples:
                body_sig = np.zeros(n_samples)
                for i in range(body_period, n_samples):
                    body_sig[i] = y[i - body_period]
                y = y + body_sig * body_resonance * 0.45

        return y / max(np.max(np.abs(y)), 1e-10)

    def _generate_pad(self, params: Dict[str, float],
                      duration_ms: float) -> np.ndarray:
        n_samples = int(self.sr * duration_ms / 1000.0)
        t = np.arange(n_samples) / self.sr

        freq = params.get("freq", 220.0)
        attack = params.get("attack", 0.3)
        release = params.get("release", 0.5)
        detune = params.get("detune", 0.01)
        warmth = params.get("warmth", 0.5)
        chorus_amount = params.get("chorusAmount", 0.0)
        filter_sweep = params.get("filterSweep", 0.0)
        evolution_rate = params.get("evolutionRate", 0.0)
        n_voices = 4

        # Unison saw-like voices (richer than pure sines for pads)
        y = np.zeros(n_samples)
        for i in range(n_voices):
            d = 1.0 + (i - n_voices / 2) * detune / n_voices
            # Use saw wave for richer harmonics (like C++ PolyBLEP saws)
            saw = 2.0 * (np.cumsum(np.full(n_samples, freq * d)) / self.sr % 1.0) - 1.0
            y += saw
        y /= n_voices

        # Warmth filter with evolution LFO modulating cutoff
        # warmth=0 -> bright (4500Hz), warmth=1 -> warm (500Hz)
        warmth_base = 500.0 + (1.0 - warmth) * 4000.0

        if evolution_rate > 0.01:
            # Evolution LFO: slow modulation of filter cutoff over time
            # evolutionRate controls speed (0..1 -> 0.05..2.0 Hz)
            evo_freq = 0.05 + evolution_rate * 1.95
            evo_lfo = np.sin(2.0 * np.pi * evo_freq * t)
            # Modulate warmth cutoff: sweep range depends on filter_sweep
            sweep_depth = 0.3 + filter_sweep * 0.5
            warmth_cutoff = warmth_base * (1.0 + evo_lfo * sweep_depth)
            warmth_cutoff = np.clip(warmth_cutoff, 200.0, self.sr * 0.48)

            # Apply time-varying LP filter via one-pole approximation
            alpha = np.clip(warmth_cutoff * 2.0 * np.pi / self.sr, 0.001, 0.999)
            filtered = np.zeros(n_samples)
            filtered[0] = y[0]
            for i in range(1, n_samples):
                a = alpha[i]
                filtered[i] = a * y[i] + (1.0 - a) * filtered[i - 1]
            y = filtered

            # Second evolution LFO at different rate for spectral morphing
            evo_lfo2 = np.sin(2.0 * np.pi * evo_freq * 1.7 * t)
            # Spectral morphing: blend between filtered and unfiltered content
            morph_amount = 0.5 + 0.5 * evo_lfo2 * filter_sweep
            # Apply as subtle amplitude modulation of harmonic content
            y = y * (0.7 + 0.3 * morph_amount)
        else:
            # Static warmth filter (original behavior)
            kernel_size = max(2, int(self.sr / warmth_base / 2))
            kernel = np.ones(kernel_size) / kernel_size
            y = np.convolve(y, kernel, mode='same')

            # Filter sweep: modulate warmth filter with slow LFO
            if filter_sweep > 0.01:
                sweep_lfo = np.sin(2.0 * np.pi * 0.3 * t) * filter_sweep * 0.25
                y = y * (1.0 + sweep_lfo)

        # Chorus: modulated delay approximation
        if chorus_amount > 0.01:
            chorus_delay_ms = 8.0  # base delay in ms
            mod_depth_ms = chorus_amount * 3.5
            lfo = np.sin(2.0 * np.pi * 0.9 * t)
            delay_samples = ((chorus_delay_ms + lfo * mod_depth_ms) * 0.001 * self.sr).astype(int)
            delay_samples = np.clip(delay_samples, 1, n_samples - 1)
            chorus_sig = np.zeros(n_samples)
            for i in range(n_samples):
                idx = i - delay_samples[i]
                if 0 <= idx < n_samples:
                    chorus_sig[i] = y[idx]
            wet = chorus_amount * 0.5
            y = y * (1.0 - wet) + chorus_sig * wet

        # Amplitude envelope
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
        grain_size = params.get("grainSize", 0.03)
        noise_color = params.get("noiseColor", 0.5)
        spectral_tilt = params.get("spectralTilt", 0.0)
        pitchedness = params.get("pitchedness", 0.0)
        pitch = params.get("pitch", 0.5)

        # Base frequency for pitched grains (pitch 0..1 -> 80..800 Hz)
        base_freq = 80.0 + pitch * 720.0

        # Granular noise: colored noise grains
        white = np.random.randn(n_samples)
        pink = np.cumsum(np.random.randn(n_samples))
        pink = pink - np.linspace(pink[0], pink[-1], n_samples)
        pink = pink / max(np.max(np.abs(pink)), 1e-10)
        noise = noise_color * white + (1.0 - noise_color) * pink

        # Grain windowing: apply grain-sized Hann windows
        grain_samples = max(10, int(grain_size * self.sr))
        grain_interval = max(1, int(1.0 / max(density * 200.0, 1.0) * self.sr))
        y = np.zeros(n_samples)
        grain_rng = np.random.RandomState(42)
        pos = 0
        while pos < n_samples:
            end = min(pos + grain_samples, n_samples)
            length = end - pos
            window = 0.5 * (1.0 - np.cos(2.0 * np.pi * np.arange(length) / length))
            grain_amp = 1.0 / max(1.0, np.sqrt(density * 40.0))

            # Noise grain
            noise_grain = noise[pos:end] * window * grain_amp

            # Pitched grain: sine at base_freq with slight random detune
            if pitchedness > 0.01:
                detune_factor = 1.0 + grain_rng.uniform(-0.03, 0.03) * movement
                grain_freq = base_freq * detune_factor
                grain_t = np.arange(length) / self.sr
                pitched_grain = np.sin(2.0 * np.pi * grain_freq * grain_t) * window * grain_amp
                # Blend noise and pitched grains based on pitchedness
                y[pos:end] += (1.0 - pitchedness) * noise_grain + pitchedness * pitched_grain
            else:
                y[pos:end] += noise_grain

            pos += grain_interval

        # Movement modulation
        mod = 1.0 + movement * 0.5 * np.sin(2.0 * np.pi * 0.5 * t)
        y = y * mod

        # Spectral tilt: HP (positive tilt) or LP (negative tilt)
        if abs(spectral_tilt) > 0.05:
            tilt_cutoff = 300.0 + abs(spectral_tilt) * 8000.0
            kernel_size = max(2, int(self.sr / tilt_cutoff / 2))
            kernel = np.ones(kernel_size) / kernel_size
            smoothed = np.convolve(y, kernel, mode='same')
            if spectral_tilt > 0:
                # HP: subtract LP version
                y = y - smoothed * abs(spectral_tilt)
            else:
                # LP: blend toward smoothed
                y = y * (1.0 - abs(spectral_tilt)) + smoothed * abs(spectral_tilt)

        # Brightness LP filter
        if brightness < 0.95:
            kernel_size = max(2, int((1.0 - brightness) * 100))
            kernel = np.ones(kernel_size) / kernel_size
            y = np.convolve(y, kernel, mode='same')

        env = np.minimum(t / 0.1, 1.0) * np.exp(-t / max(duration_ms / 2000, 0.1))
        y = y * env
        return y / max(np.max(np.abs(y)), 1e-10)

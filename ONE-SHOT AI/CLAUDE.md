# AI One-Shot Generator

Plugin JUCE (C++17) que genera one-shots de audio proceduralmente con IA basada en reglas musicales.
10 instrumentos (Kick, Snare, HiHat, Clap, Perc, Bass808, Lead, Pluck, Pad, Texture) x 9 generos (Trap, HipHop, Techno, House, Reggaeton, Afrobeat, RnB, EDM, Ambient).

## Estado actual (2026-04-07)

UniversalSynth con 88 parametros y 4 capas paralelas. **Capa Modal habilitada** (fix de estabilidad SVFilter).
Pipeline v3 completo. **One-Shot Match v3**: Spectral Matching con CMA-ES + Multi-Resolution Mel Spectrogram Loss. Busca en los 88 params completos sin depender de clasificador.

## Arquitectura

- **Header-only** C++17 JUCE plugin — no modificar .jucer
- **UniversalSynth** — motor de sintesis unico para generacion y matching
  - 4 capas paralelas: Tonal (osc+FM+additive+unison+sub) | Noise (colored+bursts+granular) | Modal/KS (resonators+Karplus-Strong) | Transient (clicks+snaps+impulses)
  - 88 parametros totales, todos con gates (capa off = CPU ahorrada)
  - Reemplaza tanto el antiguo SynthEngine (10 synths especializados) como OneShotMatchSynth (106 params)
- **Deterministic** — seed-based RNG (std::mt19937)
- Flujo: GenerationRequest -> ParameterGenerator -> UniversalGenreRules -> UniversalSynthParams -> UniversalSynth -> EffectChain -> AudioBuffer
- Efectos DSP: EffectChain aplica 9 efectos post-sintesis (EQ, Compressor, Distortion, Bitcrusher, RingMod, Chorus, Phaser, Delay, Reverb)
- Master post-processing: dcBlock -> softClip -> truePeakLimiter -> normalize

## Estructura de codigo

```
Source/
├── Params/          # Enums, structs, GenerationResult (universalParams + legacyParams)
├── DSP/             # Oscillator, SynthFilter, Envelope, NoiseGenerator, DelayLine, Saturator, DCBlocker
├── UniversalSynth/  # Motor de sintesis universal
│   ├── UniversalSynthParams.h  # 88 params + bounds + toArray/fromArray
│   ├── UniversalSynth.h        # 4-layer render engine
│   ├── ModalBank.h             # Modal resonators + Karplus-Strong
│   └── UniversalGenreRules.h   # 90 instrument x genre presets
├── SynthEngine/     # Legacy 10 synths (kept for reference, not used in main path)
├── Effects/         # 9 effects + EffectChain + EffectParams
├── AI/              # ParameterGenerator + GenreRules (legacy) + MutationAxes
├── OneShotMatch/    # Spectral matching (analysis-by-synthesis)
│   ├── OneShotMatchDescriptors.h  # Descriptors + MelFilterbank + Mel/Envelope/Pitch loss functions
│   ├── OneShotMatchOptimizer.h    # v5: Hierarchical 4-phase CMA-ES + phase-specific losses
│   └── OneShotMatchEngine.h       # Orchestrator (load -> analyze -> CMA-ES optimize -> result)
├── WebUI/
│   ├── OneShotWebUI.h             # Generator tab + tab system
│   ├── OneShotMatchWebUI.h        # Match tab
│   └── OneShotSynthWebUI.h        # Synth Editor tab (88 knobs)
├── Test/            # TestWavExporter
├── WebViewPluginDemo.h  # Main AudioProcessor + HTTP API
└── Main.cpp
```

## UniversalSynth parametros (88 total)

- **Global (6):** oscType, basePitch, duration, masterGain, stereoWidth, pan
- **Pitch Engine (8):** pitchEnvDepth/Fast/Slow/Balance, pitchHoldTime, pitchBounce, pitchWobble, wobbleRate
- **Layer A — Tonal (18):** tonalLevel, bodyHarmonics, fmDepth/Ratio/Decay, additiveAmt, harmonic2-5, inharmonicity, unisonVoices/Detune/Drift, phaseDistort/Decay, subLevel/Detune
- **Layer B — Noise (14):** noiseLevel, noiseColor, noiseFilterFreq/Q, noiseDecay, burstCount/Spacing, noiseAttack, residualAmt/Level, noiseHP, noiseStereo, noiseEvolution, granularDensity
- **Layer C — Modal/KS (12):** modalLevel, modalMode, numModes, modeDecay/Spread/RatioBase/Damping, ksFeedback/Damping/Brightness/PickPosition/BodyResonance
- **Layer D — Transient (8):** transientLevel, clickType, clickFreq/Decay/Width, snapAmount, transientSampleAmt, topNoise
- **Amplitude Envelope (8):** ampAttack, ampPunchDecay, ampBodyDecay, ampPunchLevel, envSustainLevel/Time, envRelease, envCurve
- **Filter Chain (8):** filterCutoff/Reso, filterSweepAmt/Start/End, formantAmt, formantFreq1/2
- **Effects/Dynamics (6):** reverbAmt/Decay, chorusAmt, satAmount, satType, compAmount

## Modos del plugin

- **Generator Mode**: Seleccionar instrumento + genero -> UniversalGenreRules proporciona params base -> UniversalSynth renderiza
- **Match Mode**: Cargar WAV -> 4-phase hierarchical optimization (envelope→timbre→polish) -> UniversalSynth reconstruye
- **Synth Editor Mode**: Control manual de los 88 params con knobs rotatorios, render directo, load desde Generator/Match
- UI con tabs: [Generator] [One-Shot Match] [Synth]
- API endpoints: `api/synth/render` (88 params -> WAV), `api/synth/last-params` (JSON array)
- namespace: `oneshotmatch` para match, `universalsynth` para synth
- Score formula: `100 x exp(-dist x 2.0)` (mel loss scale)

## Match system v7 — Professional-Grade Matching with Spectral Residual (2026-04-09)

**Arquitectura: "Measure + Optimize + Compensate"**

Insight clave: ningun synth parametrico modela todo. Sistemas profesionales (iZotope, Melodyne, DDSP)
compensan el gap con procesamiento espectral. v7 agrega Spectral Residual Compensation (Phase 4).

- **Phase 0: Direct Analysis** (instant)
  - MEDIR y FIJAR: basePitch, duration (±2%), masterGain, pitch/amp envelope
  - YIN pitch detection (reemplaza autocorrelacion — elimina octave-locking)
  - filterCutoff min=800Hz, subDetune ±3st

- **Phase 1: OscType Screen** (~14 evals)

- **Phase 2: BIPOP-CMA-ES Timbral** (~45 params, budget-based)
  - BIPOP restarts: alterna large-pop (exploracion) y small-pop (explotacion)
  - Loss (evaluateTimbre): `0.45*STFT_7res_mildweight + 0.45*Mel + 0.10*RMS`
  - Mild perceptual weighting: `sqrt(A-weight)` con floor=0.4 (no mata graves)

- **Phase 3: Global NM Polish** (top-40 params, 10 rondas)
  - Full composite loss: `0.30*STFT + 0.25*Mel + 0.20*Env + 0.10*Attack + 0.05*Pitch + 0.10*RMS`
  - Envelope loss incluye derivative matching (slope, no solo valor)
  - Attack loss incluye energy matching (no solo forma normalizada)

- **Phase 4: Spectral Residual Compensation** (NEW — 95%+ match)
  - STFT(ref) - STFT(synth) con half-wave rectification (solo agrega lo que FALTA)
  - Filtrado: mediana temporal, cap ±12dB, fase de referencia
  - Blend slider: 0.0 = pure synth, 0.7 = default hybrid, 1.0 = near-perfect
  - `compensatedBuffer` almacenado separado del `matchedBuffer` (synth puro)
  - API: `api/match/blend?value=0.7`, `api/match/audio` sirve compensated si blend > 0

- **Mejoras en loss functions v7:**
  - `aWeightForFreq()`: mild perceptual weight = sqrt(A-weight) floor=0.4 en computeLinearSTFTLoss
  - `computeEnvelopeDistance`: value L1 + derivative L1 (0.65/0.35)
  - `computeAttackWaveformLoss`: shape L1 + energy diff (0.70/0.30)
  - YIN en `computePitchContourLoss` (inline per-frame)
- Side-channel data: wavetable, residual noise, transient sample, harmonic phases, spectral envelope
- Score formula: `100 x exp(-dist x 2.0)`

## Python training pipeline

- `synth_bridge.py`: Python mirror de UniversalSynth (88 params, 4 layers)
- `model.py`: MLP 24->256->128->88 con Sigmoid
- `03_optimize_params.py`: DE optimization con UNIVERSAL_BOUNDS (88 params)
- `05_train_model.py`: Training con feature-space morphing
- `06_export_onnx.py`: ONNX export (dynamic dimensions)
- `07_train_quality_scorer.py`: Quality scorer (33 features -> score)

## Convenciones

- Idioma del usuario: espanol
- JUCE coding style (espacios antes de parentesis, etc.)
- Reggaeton = maxima "pegada" (punch/click)
- Auto-lock attack en instrumentos percusivos
- Compilar en **Release** para velocidad — Debug es 5-10x mas lento

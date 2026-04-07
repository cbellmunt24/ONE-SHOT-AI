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
│   ├── OneShotMatchOptimizer.h    # v3: CMA-ES (88 params) + Mel spectrogram loss + NM polish
│   └── OneShotMatchEngine.h       # Orchestrator (load -> analyze -> CMA-ES optimize -> result)
├── WebUI/
│   ├── OneShotWebUI.h             # Generator tab
│   └── OneShotMatchWebUI.h        # Match tab
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
- **Match Mode**: Cargar WAV -> mel spectrogram analysis -> CMA-ES optimiza 88 params -> UniversalSynth reconstruye
- UI con tabs: [Generator] [One-Shot Match]
- namespace: `oneshotmatch` para match, `universalsynth` para synth
- Score formula: `100 x exp(-dist x 2.0)` (mel loss scale)

## Match system v3 — Spectral Matching (2026-04-07)

**Arquitectura: "Direct Spectral Comparison + CMA-ES"**

Cambio fundamental: la loss function compara AUDIO REAL (mel spectrogram), no descriptores escalares.

- **Loss function principal: Multi-Resolution Mel Spectrogram**
  - 3 resoluciones FFT: 512, 1024, 2048 (pesos 0.25/0.5/0.25)
  - 128 bandas Mel (escala perceptual humana)
  - Spectral Convergence (Frobenius norm ratio) + Log Magnitude L1
  - Composite: `0.7 * melLoss + 0.15 * (1 - envCorrelation) + 0.15 * pitchContourLoss`
- **Optimizer: CMA-ES** (Covariance Matrix Adaptation Evolution Strategy)
  - Busca TODOS los 88 parametros (no solo 12-18)
  - lambda=20, 250 generaciones = 5000 evaluaciones, ~75 segundos
  - Jacobi eigendecomposition cada 9 generaciones para covariance matrix 88x88
  - No depende de clasificador de instrumento para busqueda
  - Sin params "locked" — todos optimizables con inicializacion inteligente
- **Inicializacion inteligente:**
  - 60% descriptor guess + 40% preset seed (como punto de partida, no lock)
  - OscType screening: prueba todas las formas de onda, elige la mejor
  - Warm-start desde KNN de matches anteriores
- **NM polish** en top 30 params mas sensibles (post CMA-ES)
- **Clasificador de instrumento:** mantenido solo para inicializacion y UI display
- Side-channel data: wavetable, residual noise, transient sample, harmonic phases, spectral envelope
- **ModalBank habilitada:** fix SVFilter (freq clamp 0.45×SR + Q dampening near Nyquist)
- Score formula: `100 x exp(-dist x 2.0)` (mel loss scale)

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

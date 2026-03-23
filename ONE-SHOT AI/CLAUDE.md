# AI One-Shot Generator

Plugin JUCE (C++17) que genera one-shots de audio proceduralmente con IA basada en reglas musicales.
10 instrumentos (Kick, Snare, HiHat, Clap, Perc, Bass808, Lead, Pluck, Pad, Texture) × 9 géneros (Trap, HipHop, Techno, House, Reggaeton, Afrobeat, RnB, EDM, Ambient).

## Estado actual
Fases 1-4 completadas. Pipeline v3 completo.
One-Shot Match mode integrado y funcional (tab en WebUI).
Iterando calidad de match (mix balance, optimizer tuning, score display).

## Arquitectura
- **Header-only** — no modificar .jucer
- **std::variant + std::visit** — dispatch polimórfico sin herencia virtual
- **Deterministic** — seed-based RNG (std::mt19937)
- Flujo: GenerationRequest → ParameterGenerator → GenerationResult → SynthEngine → EffectChain → AudioBuffer
- Efectos DSP: EffectChain aplica 9 efectos (EQ, Compressor, Distortion, Bitcrusher, RingMod, Chorus, Phaser, Delay, Reverb)
- Master post-processing: softClipBuffer + normalizeBuffer en SynthEngine.h
- Test: TestWavExporter en WebViewPluginDemo.h constructor genera 90 .wav al arrancar

## Estructura de código
```
Source/
├── Params/          # Espacio paramétrico (enums, structs, variant)
├── DSP/             # Utilidades DSP (oscillator, filter, envelope, noise, delay, saturator)
├── SynthEngine/     # 10 sintetizadores + dispatcher + utils
├── Effects/         # 9 efectos DSP + EffectChain + EffectParams
├── AI/              # ParameterGenerator + GenreRules (90 perfiles + reglas de efectos)
├── OneShotMatch/    # Sistema One-Shot Match (analysis-by-synthesis universal)
│   ├── OneShotMatchDescriptors.h  # Descriptores + distancia + gap analysis
│   ├── OneShotMatchSynth.h        # Sintetizador universal (76 params)
│   ├── OneShotMatchOptimizer.h    # Multi-pass DE + Nelder-Mead + warm-start
│   └── OneShotMatchEngine.h       # Orquestador con persistencia
├── WebUI/
│   ├── OneShotWebUI.h             # Generator tab (con tab system)
│   └── OneShotMatchWebUI.h        # One-Shot Match tab
├── Test/            # TestWavExporter
├── WebViewPluginDemo.h  # AudioProcessor principal + API endpoints
└── Main.cpp
```

## Modos del plugin
- **Generator Mode**: Sistema existente ML → SynthEngine → Audio (no modificar)
- **One-Shot Match Mode**: Referencia → Descriptores → Optimización multi-pass → Reconstrucción por síntesis
  - 95 params (22 core + 73 extensiones en 24 módulos adaptativos)
  - v1-v3: FM, resonance, wobble, snap, comb, multiband sat, phase distort, additive (8 harmonics), multi-reson, noise shape, EQ, env complex, stereo, unison, formant, transient layer, reverb
  - v4 (10 params): mix levels optimizables (body/sub/click/top), filter sweep dinámico, sub pitch independiente, formant Q
  - v5 (9 params): residual noise capture, harmonics 5-8, spectral envelope matching, sub wavetable, transient sample capture
  - 13 formas de onda: sine, tri, saw, square, pulse25, pulse12, half-rect, abs-sine, parabolic, staircase, double-sine, clipped-sine, **wavetable** (extraída de referencia)
  - OscType screening: evalúa las 12 formas antes de optimizar, distribuye población en top 3
  - Warm-start desde matches previos + K-NN global learning desde historial
  - Gap analysis en 2 pasadas (activación incremental de extensiones)
  - Descriptores: ~45 escalares + 64 spectrotemporales + 8 micro-transient + stereo
  - Distancia perceptual: bandas Mel + A-weighting en comparaciones espectrales
  - Pitch detection: 20-500 Hz (autocorrelación con interpolación parabólica)
  - Side-channel data: wavetable (32 frames, multi-cycle, full duration), residual noise (spectral subtraction + own envelope), transient sample (5ms), spectral envelope, harmonic phases (8 harmonics)
- UI con tabs: [Generator] [One-Shot Match]
- namespace: `oneshotmatch`, API: `/api/match/`
- Score formula: `100 × exp(-dist × 0.12)` — escala lineal sobre distancia real
- Optimizer: pop=120, iters=600, Nelder-Mead=60, stagnation=30/0.0003, targetDist=0.3
- Mix levels: optimizables (body/sub/click/top como params, ya no hardcoded)
- Distance weights: envelope=7.0, sub energy=2.5, perceptual A-weighting en bandas
- Compilar en **Release** para velocidad — Debug es 5-10x más lento

## Convenciones
- Idioma del usuario: español
- JUCE coding style (espacios antes de paréntesis, etc.)
- Reggaeton = máxima "pegada" (punch/click)
- Auto-lock attack en instrumentos percusivos
- Diseñado para ser upgradeable a ML (misma interfaz GenerationResult)

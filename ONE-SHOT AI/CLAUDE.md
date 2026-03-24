# AI One-Shot Generator

Plugin JUCE (C++17) que genera one-shots de audio proceduralmente con IA basada en reglas musicales.
10 instrumentos (Kick, Snare, HiHat, Clap, Perc, Bass808, Lead, Pluck, Pad, Texture) × 9 géneros (Trap, HipHop, Techno, House, Reggaeton, Afrobeat, RnB, EDM, Ambient).

## Estado actual
Fases 1-4 completadas. Pipeline v3 completo.
One-Shot Match mode integrado y funcional (tab en WebUI).
Iterando calidad de match — sesión 2026-03-23: rebalanceo de pesos, fix pitch detection, nuevos descriptores.

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
  - 106 params (22 core + 84 extensiones en 30 módulos adaptativos)
  - v1-v3: FM, resonance, wobble, snap, comb, multiband sat, phase distort, additive (8 harmonics), multi-reson, noise shape, EQ, env complex, stereo, unison, formant, transient layer, reverb (FDN+allpass)
  - v4 (10 params): mix levels optimizables (body/sub/click/top), filter sweep dinámico, sub pitch independiente, formant Q
  - v5 (9 params): residual noise capture, harmonics 5-8, spectral envelope matching, sub wavetable, transient sample capture
  - v6 (11 params): pitch bounce + hold, click types (noise/impulse/FM burst/chirp), master saturation (3 modes), sub phase offset, internal compressor (amount/ratio/attack/release), sub crossover frequency
  - 13 formas de onda: sine, tri, saw, square, pulse25, pulse12, half-rect, abs-sine, parabolic, staircase, double-sine, clipped-sine, **wavetable** (extraída de referencia)
  - OscType screening: evalúa las 12 formas antes de optimizar, distribuye población en top 3
  - Warm-start desde matches previos + K-NN global learning desde historial
  - Gap analysis en 2 pasadas (activación incremental de extensiones)
  - Descriptores: ~50 escalares + harmonicProfile[6] + 16 body hi-res bands + 64 spectrotemporales + 8 micro-transient + stereo
  - Nuevos descriptores (2026-03-23): harmonicProfile (6 armónicos), subHarmonicRatio, noiseSpectralCentroid, spectralCrest
  - Distancia perceptual: bandas Mel + A-weighting en comparaciones espectrales
  - Pitch detection: 20-500 Hz (autocorrelación con interpolación parabólica, ventana adaptativa min 2.5 ciclos de f0)
  - Side-channel data: wavetable (32 frames, multi-cycle, full duration), residual noise (spectral subtraction + own envelope), transient sample (5ms), spectral envelope, harmonic phases (8 harmonics)
- UI con tabs: [Generator] [One-Shot Match]
- namespace: `oneshotmatch`, API: `/api/match/`
- Score formula: `100 × exp(-dist × 0.35)` — curva más estricta, 70%+ = buen match perceptual
- Optimizer: pop=60, iters=250, Nelder-Mead=60, stagnation=15 + partial restart (2x), targetDist=1.5
- Optimizer strategy: DE/current-to-best/1 híbrido (greediness ramps 0.3→1.0 según progreso)
- Seeding mejorado: harmonicProfile → bodyHarmonics/harmonicEmphasis/additive, subHarmonicRatio → subLevel/subDetune, noiseSpectralCentroid → noiseAmount/noiseColor
- Mix levels: optimizables (body/sub/click/top como params, ya no hardcoded)
- Distance weights (rebalanceados 2026-03-23): pitch envelope=6.0, spectral=~12, spectrotemporal=8.0, transient=~8, micro-transient=5.0, envelope=~7 (reducido de 13.5), body hi-res=6.0, harmonic profile=3.0
- Bug fix crítico: pitch detection usaba ventana fija 4ms (solo detectaba >500Hz) → ahora adaptativa basada en f0
- Compilar en **Release** para velocidad — Debug es 5-10x más lento

## Convenciones
- Idioma del usuario: español
- JUCE coding style (espacios antes de paréntesis, etc.)
- Reggaeton = máxima "pegada" (punch/click)
- Auto-lock attack en instrumentos percusivos
- Diseñado para ser upgradeable a ML (misma interfaz GenerationResult)

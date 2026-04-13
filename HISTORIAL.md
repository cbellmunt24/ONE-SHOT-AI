# ONE-SHOT AI — Historial de desarrollo

## Fase 1: Diseño inicial (2026-03)
- Plugin JUCE C++17 con WebView2 UI
- 10 instrumentos × 9 géneros = 90 combinaciones
- Arquitectura header-only, std::variant dispatch
- Parámetros: BaseSoundParams + InstrumentParams específicos por instrumento
- 6 fases de desarrollo planificadas (Params → Synth → AI → Effects → UI → Iteration)

## Fase 2: Motor de síntesis v1
- 10 sintetizadores especializados: KickSynth, SnareSynth, HiHatSynth, ClapSynth, PercSynth, Bass808Synth, LeadSynth, PluckSynth, PadSynth, TextureSynth
- Cada uno con 6-12 parámetros específicos
- EffectChain con 9 efectos (EQ, Compressor, Distortion, Bitcrusher, RingMod, Chorus, Phaser, Delay, Reverb)
- Post-processing: DC block → soft clip → true peak limiter → normalize

## Fase 3: Pipeline ML v1-v3
- Pipeline Python de 7 scripts: extract features → build profiles → optimize params → export rules → train model → export ONNX → quality scorer
- v1: Parámetros básicos, muchos NaN
- v2: Fix NaN, parámetros expandidos (5-8 por instrumento)
- v3 (final): 86 combos optimizados, 0 NaN, val_loss=0.000029, quality 100%/98%
- Modelo MLP: 24 inputs (10 instr + 9 genre + 5 sliders) → 35 outputs
- ONNX integrado en C++ con fallback a reglas

## Fase 4: Kick Match (primer sistema de match)
- Sistema original limitado a kicks
- 13 parámetros de síntesis
- 15 descriptores de audio
- DE (pop=30, 100 iters) + Nelder-Mead (50 iters)
- Distancia ponderada con 7 pesos
- API REST: 10 endpoints

## Fase 5: One-Shot Match universal (v1-v6)
- Expansión de Kick Match a cualquier instrumento
- 106 parámetros (22 core + 84 extensiones en 6 versiones)
- ~50 descriptores + spectrotemporal + harmonic profile
- Gap analysis en 2 pasadas para activación incremental
- OscType screening, warm-start, K-NN learning
- Side-channel data: wavetable, residual noise, transient sample, harmonic phases, spectral envelope

## Fase 6: UniversalSynth (2026-03-24) — Estado actual
- Reemplazo completo: 10 synths especializados + OneShotMatchSynth → 1 UniversalSynth
- 88 parámetros, 4 capas paralelas (Tonal + Noise + Modal/KS + Transient)
- Sintesis modal (12 resonadores BP) + Karplus-Strong (nuevo)
- Un solo motor para generación Y matching
- 90 presets instrumento×género en UniversalGenreRules.h
- Pipeline Python reescrito para 88 params universales

## Especificaciones de código (legacy)
- Separación en 3 módulos: síntesis procedural, parámetros IA, WebUI
- La IA genera vectores de parámetros, NO formas de onda
- Variables musicales claras: freqBase, pitchDecay, brillo, cuerpo, textura, movimiento
- Estilo JUCE: espacios antes de paréntesis, header-only

## Incidentes
- 149 samples perdidos por bug octal en bash (números con ceros a la izquierda)
- Fix: usar 10#$var para forzar decimal

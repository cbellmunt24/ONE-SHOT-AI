# AI One-Shot Generator

Plugin JUCE (C++17) que genera one-shots de audio proceduralmente con IA basada en reglas musicales.
10 instrumentos (Kick, Snare, HiHat, Clap, Perc, Bass808, Lead, Pluck, Pad, Texture) × 9 géneros (Trap, HipHop, Techno, House, Reggaeton, Afrobeat, RnB, EDM, Ambient).

## Estado actual
Fases 1-4 completadas. Iterando calidad de síntesis.
El usuario prepara una tabla de feedback específico por instrumento/género para ajustar la síntesis.

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
├── Test/            # TestWavExporter
├── WebViewPluginDemo.h  # AudioProcessor principal
└── Main.cpp
```

## Convenciones
- Idioma del usuario: español
- JUCE coding style (espacios antes de paréntesis, etc.)
- Reggaeton = máxima "pegada" (punch/click)
- Auto-lock attack en instrumentos percusivos
- Diseñado para ser upgradeable a ML (misma interfaz GenerationResult)

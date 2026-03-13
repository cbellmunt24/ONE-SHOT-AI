# ONE-SHOT AI — Estado del Plugin y Mejoras (2026-03-09)

## Resumen Pipeline ML v3 (final)

| Fase | Script | Estado | Resultado |
|------|--------|--------|-----------|
| 3 | 01_extract_features + 01b | COMPLETADA | 16,927 JSONs con features + windowed_features |
| 3.5 | 02_build_profiles | COMPLETADA | 87 perfiles (68/90 combos con samples) |
| 4 | 03_optimize_params | COMPLETADA (v3) | 86 combos, 0 NaN, params expandidos + synths mejorados |
| 4 | 03b_learn_mutation_axes | COMPLETADA (v3) | PCA por instrumento, MutationAxes.h regenerado |
| 4 | 04_export_genre_rules | COMPLETADA (v3) | GenreRules_calibrated.h (854 lineas) |
| 5 | 05_train_model | COMPLETADA (v3) | MLP 13,731 params, val_loss=0.000029 |
| 6 | 06_export_onnx | COMPLETADA (v3) | oneshot_param_predictor.onnx (10.2KB), verificado |
| 7 | 07_train_quality_scorer | COMPLETADA (v3) | 100% reales OK, 98% malos detectados, ONNX OK |
| 8 | Expansion params + mejora synths | COMPLETADA | synth_bridge, C++ synths, ONNX integrado |

### Archivos generados

```
Training/data/
  optimized_params/    86 JSONs con parametros optimos (v3 - synths mejorados)
  mutation_axes/       PCA axes + MutationAxes.h para C++
  profiles/            87 perfiles estadisticos
  models/
    oneshot_param_predictor.pt     Modelo PyTorch (MLP)
    oneshot_param_predictor.onnx   Modelo ONNX para C++ (10.2KB)
    quality_scorer.pt              Quality scorer PyTorch
    quality_scorer.onnx            Quality scorer ONNX
    feature_space_map.png          Mapa PCA del espacio de parametros
  dataset/
    training_data.npz              1,285,582 filas
    quality_scorer_data.npz        18,287 filas
  GenreRules_calibrated.h          Header C++ con parametros calibrados (854 lineas)

ONE-SHOT AI/
  Resources/
    oneshot_param_predictor.onnx   Copiado para el plugin
    quality_scorer.onnx            Copiado para el plugin
  Source/AI/
    GenreRules.h                   REEMPLAZADO con version ML-calibrada
    MutationAxes.h                 NUEVO - ejes de mutacion PCA
    ParameterGenerator.h           ACTUALIZADO con inferencia ONNX
  Source/SynthEngine/
    TextureSynth.h                 MEJORADO - pitched grains
    PluckSynth.h                   MEJORADO - FM synthesis
  Source/Params/
    InstrumentParams.h             ACTUALIZADO - nuevos params
```

---

## Resultados de Optimizacion v3

| Instrumento | Params | Dist. Media | Mejor (dist) | Peor (dist) | Conv. | NaN |
|-------------|--------|------------|--------------|-------------|-------|-----|
| **808s** | 7 | 60.4 | ambient (27.7) | house (107.8) | 7/9 | 0 |
| **kicks** | 8 | 89.5 | house (66.3) | hiphop (104.9) | 1/8 | 0 |
| **percs** | 6 | 114.9 | house (81.5) | ambient (141.9) | 2/9 | 0 |
| **plucks** | 6 | 150.4 | edm (73.7) | ambient (272.6) | 4/9 | 0 |
| **claps** | 6 | 176.9 | edm (132.9) | ambient (269.8) | 0/9 | 0 |
| **leads** | 7 | 181.8 | afrobeat (107.0) | ambient (300.5) | 5/9 | 0 |
| **snares** | 7 | 191.1 | hiphop (138.1) | techno (347.9) | 2/9 | 0 |
| **hihats** | 6 | 221.8 | rnb (155.4) | afrobeat (309.8) | 0/9 | 0 |
| **textures** | 8 | 238.4 | edm (169.6) | house (417.8) | 0/9 | 0 |
| **pads** | 8 | 300.8 | edm (104.7) | rnb (462.3) | 1/6 | 0 |

### Evolucion v1 -> v2 -> v3

| Instrumento | v1 (params basicos) | v2 (params expandidos) | v3 (synths mejorados) |
|-------------|--------------------|-----------------------|----------------------|
| **808s** | 125.0 | 61.9 | **60.4** |
| **kicks** | 87.8 | 89.8 | **89.5** |
| **percs** | 137.9 (4 NaN) | 122.5 (4 NaN) | **114.9 (0 NaN)** |
| **plucks** | 576.1 | 156.2 | **150.4** |
| **claps** | 355.6 | 174.2 | **176.9** |
| **leads** | OVERFLOW | 181.8 | **181.8** |
| **snares** | 235.7 (2 NaN) | 201.1 (2 NaN) | **191.1 (0 NaN)** |
| **hihats** | 218.1 (3 NaN) | 225.9 (3 NaN) | **221.8 (0 NaN)** |
| **textures** | OVERFLOW | 312.0 | **238.4** |
| **pads** | OVERFLOW | 271.7 | **300.8** |

**Total NaN: 9 -> 9 -> 0**

---

## Cambios realizados

### Fix NaN (9 combinaciones)
- Causa real: perfiles con NaN en `noise_ratio`/`harmonic_ratio` (HPSS fallo en samples reales)
- Fix: guards NaN en `_legacy_timbral_distance` y `_perceptual_feature_distance`
- Sanitizacion en `extract_features_fast`: reemplaza NaN con 0.0

### Mejoras de synths

**TextureSynth** (dist 312 -> 238, -24%):
- Python: nuevo param `pitchedness` (0=noise, 1=pitched sine grains) + `pitch`
- C++: pitched grain generation usando Oscillator (Sine mode)

**PluckSynth** (dist 156 -> 150):
- Python: nuevo param `fmAmount` (0=Karplus-Strong, 1=FM dominant)
- C++: FM synthesis con carrier + modulator (ratio 2:1 y 3:1)

**PadSynth** (dist 272 -> 301):
- Python: nuevo param `evolutionRate` (LFO modulando filter cutoff)
- C++: ya tenia `evolutionRate` implementado

### Integracion C++

1. **ONNX models** copiados a `ONE-SHOT AI/Resources/`
2. **GenreRules.h** reemplazado con 86 combos ML-calibrados (854 lineas)
3. **MutationAxes.h** integrado en `Source/AI/`
4. **ParameterGenerator.h** actualizado:
   - `#if USE_ONNX_INFERENCE` para compilacion condicional
   - `loadONNXModels()`, `predictFromONNX()`, `scoreQuality()`
   - Fallback automatico a reglas si ONNX no disponible
   - `#include "MutationAxes.h"`

---

## Que queda por hacer

| Tarea | Esfuerzo | Impacto |
|-------|----------|---------|
| Compilar plugin con onnxruntime (definir USE_ONNX_INFERENCE) | 1-2h | ALTO |
| Test auditivo de los sonidos generados | 1h | ALTO |
| Mejorar PadSynth (dist 301, la peor) | 2h | MEDIO |
| Recopilar mas samples (22 combos vacios) | Variable | MEDIO |
| Optimizar ambient (peor genero en plucks/leads/claps) | 1h | MEDIO |
| Fine-tune quality scorer con nuevos datos | 30min | BAJO |

### Para compilar con ONNX
1. Descargar onnxruntime C API (prebuilt para Windows)
2. Anadir a Projucer: header path + library path
3. Definir `USE_ONNX_INFERENCE=1` en preprocessor definitions
4. Compilar — el plugin usara ML para generar parametros con fallback a reglas

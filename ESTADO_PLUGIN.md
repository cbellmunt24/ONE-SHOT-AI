# ONE-SHOT AI — Estado actual del plugin

## Resumen
Plugin JUCE C++17 que genera one-shots de audio con IA. 10 instrumentos × 9 géneros.
Dos modos: **Generator** (crear desde cero) y **Match** (reconstruir un sample por síntesis).

## Motor de síntesis: UniversalSynth

### Arquitectura
4 capas paralelas → Mixer → Filter Chain → Effects → Output

| Capa | Función | Parámetros clave | Estado |
|------|---------|-----------------|--------|
| Layer A (Tonal) | Osciladores + FM + aditivo + unísono + sub | tonalLevel, fmDepth, unisonVoices, subLevel | OK |
| Layer B (Noise) | Ruido coloreado + bursts + granular | noiseLevel, burstCount, granularDensity | OK |
| Layer C (Modal/KS) | Resonadores modales + Karplus-Strong | modalLevel, modalMode, numModes, ksFeedback | OK |
| Layer D (Transient) | Clicks + snaps + impulsos | transientLevel, clickType, snapAmount | OK |

- **88 parámetros** universales
- **13 tipos de onda** (sine, tri, saw, square, pulse, wavetable, etc.)
- Gate por capa: si xxxLevel=0, la capa se salta (ahorro CPU)

## Match System v7 — Professional-Grade (2026-04-09)

### Arquitectura: 5 fases

| Fase | Función | Estado |
|------|---------|--------|
| Phase 0 | Análisis directo: medir F0 (YIN), envelope, duration | OK |
| Phase 1 | OscType screening (14 waveforms) | OK |
| Phase 2 | BIPOP-CMA-ES timbral (~45 params) | OK |
| Phase 3 | Nelder-Mead polish (top-40 params) | OK |
| Phase 4 | **Spectral Residual Compensation** (NUEVO) | OK |

### Mejoras v7 implementadas

1. **Duration ±2%** — bounds más estrictos (antes ±10%)
2. **Loss function mejorada:**
   - Mild perceptual weighting en STFT: `sqrt(A-weight)` con floor=0.4 (no mata graves)
   - Envelope derivative matching (slope, no solo valor)
   - Attack energy matching (no solo forma normalizada)
   - Pesos: `0.30*STFT + 0.25*Mel + 0.20*Env + 0.10*Attack + 0.05*Pitch + 0.10*RMS`
3. **YIN Pitch Detection** — reemplaza autocorrelación, elimina octave-locking
4. **BIPOP-CMA-ES** — alterna large-pop (exploración) y small-pop (explotación)
5. **Spectral Residual Compensation (Phase 4):**
   - STFT 2048/hop512, half-wave rectification (solo agrega lo que falta)
   - Blend slider en UI: 0%=pure synth, 70%=default, 100%=near-perfect
   - `compensatedBuffer` separado del `matchedBuffer`
   - API: `api/match/blend?value=0.7`

### Estado del match (pendiente validar)
- [x] Compilación Release OK
- [ ] **Validar scores con kicks** — primer test dio 10% (bug A-weighting agresivo, CORREGIDO → sqrt+floor=0.4)
- [ ] Validar con snares, hihats, pads, texturas
- [ ] Verificar que el blend slider funciona correctamente en la UI
- [ ] A/B listening test con diferentes niveles de blend
- [ ] Ajustar pesos de loss si algún tipo de sonido sigue dando scores bajos

### Bug corregido (sesión actual)
- A-weighting original (floor=0.05) mataba los graves de kicks/bass
- Fix: `sqrt(A-weight)` con floor=0.4 → graves pesan al menos 40%

## Pipeline ML

| Script | Estado | Descripción |
|--------|--------|-------------|
| synth_bridge.py | Actualizado | Python mirror del UniversalSynth (88 params) |
| model.py | Actualizado | MLP 24→256→128→88 |
| 03_optimize_params.py | Actualizado | UNIVERSAL_BOUNDS (88 params) |
| 05_train_model.py | Actualizado | Dataset con 88 params |
| 06_export_onnx.py | OK | Dinámico |
| 07_train_quality_scorer.py | Actualizado | UNIVERSAL_BOUNDS |

## Compilación
- Visual Studio 18 (Insiders) / Projucer
- MSBuild: `"C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe"`
- Build **Release** para rendimiento (Debug 5-10x más lento)
- .sln en: `ONE-SHOT AI/Builds/VisualStudio2022/`

## Próximo paso: Migración a DDSP (2026-04-13)

**Decisión:** Migrar el Generator Mode y Match Mode a DDSP (Differentiable Digital Signal Processing).
Síntesis neural basada en modelos sinusoidales diferenciables — SMS + red neuronal end-to-end.

**Plan completo:** `PLAN_DDSP.md`

**Qué cambia:**
- Generator y Match usarán DDSP (encoder/decoder + SMS resíntesis)
- Match pasa de CMA-ES (~30s) a inference directa (~100ms)
- Calidad de reconstrucción: de ~85% paramétrico a ~99% sinusoidal

**Qué se mantiene:**
- UniversalSynth (88 params) para Synth Editor manual
- Todos los DSP, Effects, Params
- Samples de training (se reutilizan para entrenar DDSP)

### Pendiente (legacy, baja prioridad)
- [ ] Eliminar synths legacy cuando DDSP funcione
- [ ] Cleanup de OneShotMatchOptimizer.h (CMA-ES) cuando DDSP lo reemplace

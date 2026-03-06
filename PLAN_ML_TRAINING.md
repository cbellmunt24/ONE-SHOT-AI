# ONE-SHOT AI — Plan de Entrenamiento ML (3 Capas)

## Objetivo

Llevar la calidad de generacion de one-shots al maximo nivel posible, usando librerias de samples reales como referencia para calibrar y mejorar el motor de sintesis existente.

**Principio fundamental:** Las librerias se procesan OFFLINE en Python. El plugin C++ nunca carga samples — solo recibe parametros optimizados o un modelo ligero.

---

## Estado actual

| Fase | Estado | Notas |
|------|--------|-------|
| FASE 1: Setup | COMPLETADA | Estructura, scripts, venv, dependencias |
| FASE 2: Cargar samples | PENDIENTE — usuario | Depositar .wav en Training/libraries/ |
| FASE 3: Capa 1 — Features | PENDIENTE | Ejecutar 01 + 02 cuando haya samples |
| FASE 4: Capa 2 — Optimization | PENDIENTE | Ejecutar 03 + 04 tras Capa 1 |
| FASE 4.5: Diagnostico + Expansion | PENDIENTE | Analizar gaps, expandir params si necesario, re-optimizar |
| FASE 5: Capa 3 — ML Model | PENDIENTE | Ejecutar 05 + 06 tras Capa 2 + 4.5 |
| FASE 6: Mejoras avanzadas | PENDIENTE | Loss perceptual, mutaciones PCA, quality scoring, humanizacion |

### Proximo paso

1. Usuario deposita samples .wav en `Training/libraries/{instrumento}/{genero}/`
2. Ejecutar Capa 1:
   ```bash
   cd Training
   venv\Scripts\activate
   python scripts/01_extract_features.py
   python scripts/02_build_profiles.py --plots
   ```

### Entorno Python

- **Python:** 3.10.11 (`C:\Users\charl\AppData\Local\Programs\Python\Python310\python.exe`)
- **Venv:** `Training/venv/` (ya creado e instalado)
- **Activar:** `Training\venv\Scripts\activate`
- **Librerias instaladas:** librosa 0.11.0, numpy 2.2.6, scipy 1.15.3, torch 2.10.0+cpu, onnx 1.20.1, onnxruntime 1.23.2, matplotlib 3.10.8, pandas 2.3.3, scikit-learn 1.7.2, soundfile 0.13.1, tqdm 4.67.3

---

## Estructura de carpetas

```
ONE-SHOT AI/
├── ONE-SHOT AI/                    # Plugin C++ (NO TOCAR salvo Capa 2-3)
│   ├── Source/
│   │   ├── AI/
│   │   │   ├── ParameterGenerator.h    # Se actualiza en Capa 2 (nuevos valores)
│   │   │   ├── GenreRules.h            # Se actualiza en Capa 2 (valores calibrados)
│   │   │   └── MLParameterGenerator.h  # NUEVO en Capa 3 (inference ONNX)
│   │   ├── SynthEngine/               # Se mejora en Capa 2 si faltan parametros
│   │   └── ...
│   └── ...
│
├── Training/                       # Pipeline Python (CREADO)
│   ├── libraries/                  # 90 carpetas creadas (10 inst × 9 generos)
│   │   ├── kicks/{trap,hiphop,techno,house,reggaeton,afrobeat,rnb,edm,ambient}/
│   │   ├── snares/{...}/
│   │   ├── hihats/{...}/
│   │   ├── claps/{...}/
│   │   ├── percs/{...}/
│   │   ├── 808s/{...}/
│   │   ├── leads/{...}/
│   │   ├── plucks/{...}/
│   │   ├── pads/{...}/
│   │   └── textures/{...}/
│   │
│   ├── scripts/                    # 6 scripts + utils (ESCRITOS)
│   │   ├── 01_extract_features.py  #   Capa 1 — extrae 21 features por sample
│   │   ├── 02_build_profiles.py    #   Capa 1 — agrega perfiles + graficas
│   │   ├── 03_optimize_params.py   #   Capa 2 — differential evolution
│   │   ├── 04_export_genre_rules.py #  Capa 2 — genera GenreRules.h calibrado
│   │   ├── 05_train_model.py       #   Capa 3 — entrena MLP PyTorch
│   │   ├── 06_export_onnx.py       #   Capa 3 — exporta a ONNX + verificacion
│   │   └── utils/
│   │       ├── __init__.py
│   │       ├── audio_features.py   #   21 features (temporal, espectral, timbrico)
│   │       ├── model.py            #   Definicion MLP compartida (05/06)
│   │       ├── synth_bridge.py     #   Sintesis Python (10 instrumentos, Opcion B)
│   │       └── visualization.py    #   Graficas comparativas por genero/instrumento
│   │
│   ├── data/                       # Se llena automaticamente al ejecutar scripts
│   │   ├── features/
│   │   ├── profiles/
│   │   ├── optimized_params/
│   │   └── models/
│   │
│   ├── venv/                       # Entorno virtual Python 3.10 (INSTALADO)
│   └── requirements.txt
│
├── PLAN_ML_TRAINING.md             # Este documento
├── CLAUDE.md
└── CODE_SPECIFICATIONS.md
```

---

## CAPA 1 — Audio Feature Extraction

### Objetivo
Analizar cada sample de las librerias y extraer un perfil timbrico numerico que describa objetivamente como suena.

### Input
Samples .wav organizados en `Training/libraries/{instrumento}/{genero}/`

### Features a extraer por sample

#### Temporales
| Feature | Descripcion | Uso |
|---------|-------------|-----|
| `attack_time_ms` | Tiempo desde silencio hasta pico (ms) | Calibrar ADSR attack |
| `decay_time_ms` | Tiempo desde pico hasta -20dB (ms) | Calibrar ADSR decay |
| `total_duration_ms` | Duracion total del sample | Calibrar tailLength |
| `rms_envelope` | Curva RMS en ventanas de 5ms | Forma del volumen |
| `peak_amplitude` | Pico maximo | Calibrar volume/normalize |
| `crest_factor` | Peak/RMS ratio | Medir transient punch |
| `transient_sharpness` | Pendiente del onset (dB/ms) | Calibrar click/snap amount |

#### Espectrales
| Feature | Descripcion | Uso |
|---------|-------------|-----|
| `spectral_centroid` | Centro de masa frecuencial (Hz) | Calibrar filterCutoff, brightness |
| `spectral_rolloff_85` | Freq donde esta el 85% de la energia | Calibrar filtros |
| `spectral_bandwidth` | Ancho espectral | Calibrar resonance, Q |
| `spectral_flatness` | 0=tonal, 1=ruido | Calibrar noiseAmount, noiseMix |
| `spectral_tilt` | Pendiente del espectro (dB/octava) | Calibrar EQ, tone |
| `mfcc[0..12]` | 13 coeficientes MFCC | Timbre fingerprint |
| `fundamental_freq` | Frecuencia fundamental (Hz) | Calibrar subFreq, bodyFreq |
| `harmonic_ratio` | Energia armonicos/total | Calibrar oscType, driveAmount |
| `pitch_contour` | Evolucion del pitch en el tiempo | Calibrar pitchDrop, pitchDropTime |

#### Timbricos
| Feature | Descripcion | Uso |
|---------|-------------|-----|
| `brightness_index` | Centroid normalizado respecto a fundamental | Calibrar brillo |
| `warmth_index` | Energia <500Hz / energia total | Calibrar cuerpo, subLevel |
| `noise_ratio` | Energia no-armonica / total | Calibrar noiseAmount, wireAmount |
| `stereo_width` | Correlacion L-R (0=mono, 1=wide) | Calibrar stereoWidth, pan |
| `zero_crossing_rate` | Cruces por cero por segundo | Detectar contenido HF |

### Herramientas Python
```
librosa          — extraccion de features de audio
numpy            — calculos numericos
scipy            — analisis espectral, envolventes
soundfile        — lectura de WAV
matplotlib       — visualizacion
pandas           — tablas de features
```

### Output
- `Training/data/features/{instrumento}_{genero}_features.json`
- Un JSON por sample con todas las features
- Un CSV resumen por instrumento/genero con medias y desviaciones

### Script: `01_extract_features.py`

```
Por cada sample en libraries/{instrumento}/{genero}/:
  1. Cargar audio (mono, 44.1kHz)
  2. Extraer todas las features de la tabla
  3. Guardar en features/{instrumento}_{genero}/{filename}.json
  4. Log progreso
```

### Script: `02_build_profiles.py`

```
Por cada combinacion instrumento/genero:
  1. Cargar todos los JSON de features
  2. Calcular media, mediana, std, min, max de cada feature
  3. Calcular distribucion (para sampling posterior)
  4. Guardar perfil en profiles/{instrumento}_{genero}_profile.json
  5. Generar graficas comparativas entre generos
```

### Resultado esperado
Un **perfil timbrico de referencia** por cada combinacion instrumento/genero. Ejemplo:

```json
{
  "instrument": "kick",
  "genre": "trap",
  "num_samples_analyzed": 47,
  "profile": {
    "attack_time_ms":     { "mean": 2.1, "std": 0.8, "min": 0.5, "max": 4.2 },
    "decay_time_ms":      { "mean": 180, "std": 45, "min": 90, "max": 310 },
    "spectral_centroid":  { "mean": 142, "std": 38, "min": 80, "max": 230 },
    "fundamental_freq":   { "mean": 48.5, "std": 3.2, "min": 42, "max": 55 },
    "crest_factor":       { "mean": 8.2, "std": 1.5 },
    "noise_ratio":        { "mean": 0.12, "std": 0.05 },
    "warmth_index":       { "mean": 0.78, "std": 0.08 },
    "brightness_index":   { "mean": 0.25, "std": 0.1 },
    "pitch_drop_semitones": { "mean": 42, "std": 12 },
    "pitch_drop_time_ms": { "mean": 35, "std": 15 }
  }
}
```

---

## CAPA 2 — Parameter Optimization

### Objetivo
Encontrar los parametros del sintetizador que producen el sonido mas parecido a los perfiles reales extraidos en Capa 1.

### Proceso

#### Paso 1: Synth Bridge (Python llama al sintetizador)

Para poder optimizar, Python necesita generar audio con los mismos sintetizadores del plugin. Dos opciones:

**Opcion A — Exportar sintetizador como CLI (recomendado)**
- Crear un mini ejecutable C++ que reciba parametros por stdin/args y escriba WAV a stdout
- Python lo llama como subproceso
- Ventaja: usa EXACTAMENTE el mismo DSP que el plugin
- Coste: compilar una vez

**Opcion B — Reimplementar sintesis basica en Python**
- Replicar los sintetizadores principales en Python/NumPy
- Ventaja: todo en Python, sin compilar
- Desventaja: puede divergir del C++
- Aceptable para prototipado rapido

#### Paso 2: Funcion de coste (distancia timbrica)

```python
def timbral_distance(generated_features, target_profile):
    """
    Distancia ponderada entre features del audio generado
    y el perfil target de la libreria real.
    """
    weights = {
        'spectral_centroid':    3.0,   # Muy importante para timbre
        'fundamental_freq':     3.0,   # Critico para pitch
        'attack_time_ms':       2.5,   # Importante para transient
        'decay_time_ms':        2.0,   # Forma temporal
        'crest_factor':         2.0,   # Punch/transient
        'brightness_index':     1.5,
        'warmth_index':         1.5,
        'noise_ratio':          1.5,
        'spectral_flatness':    1.0,
        'mfcc_distance':        2.0,   # Distancia euclidea de MFCCs
        'pitch_contour_error':  2.0,   # Error en curva de pitch
    }

    distance = 0
    for feature, weight in weights.items():
        generated = generated_features[feature]
        target_mean = target_profile[feature]['mean']
        target_std = max(target_profile[feature]['std'], 1e-6)
        # Distancia normalizada por la varianza natural del genero
        distance += weight * ((generated - target_mean) / target_std) ** 2

    return distance
```

#### Paso 3: Optimizacion

```python
from scipy.optimize import differential_evolution

def optimize_kick_trap(target_profile):
    """
    Encuentra los parametros optimos de KickSynth para sonar
    como un kick de Trap real.
    """
    bounds = [
        (30, 70),      # subFreq
        (0.0, 1.0),    # clickAmount
        (0.05, 0.5),   # bodyDecay
        (10, 60),       # pitchDrop
        (0.01, 0.1),   # pitchDropTime
        (0.0, 0.5),    # driveAmount
        (0.3, 1.0),    # subLevel
        (0.05, 0.6),   # tailLength
        (200, 15000),   # filterCutoff
        (0.0, 0.8),    # filterResonance
        (0.0, 0.5),    # saturationAmount
    ]

    def objective(params):
        # 1. Generar audio con estos parametros (via synth bridge)
        audio = synth_bridge.generate_kick(params)
        # 2. Extraer features del audio generado
        features = extract_features(audio)
        # 3. Comparar con perfil target
        return timbral_distance(features, target_profile)

    result = differential_evolution(objective, bounds,
                                    maxiter=200,
                                    popsize=30,
                                    seed=42)
    return result.x  # Parametros optimos
```

#### Paso 4: Exportar a C++

### Script: `03_optimize_params.py`

```
Por cada combinacion instrumento/genero:
  1. Cargar perfil target de Capa 1
  2. Ejecutar optimizacion (differential evolution)
  3. Guardar parametros optimos en optimized_params/
  4. Generar audio de ejemplo para validacion auditiva
  5. Log: distancia final, mejora respecto a parametros actuales
```

### Script: `04_export_genre_rules.py`

```
1. Cargar todos los parametros optimizados
2. Generar nuevo GenreRules.h con valores calibrados
3. Generar diff respecto al GenreRules.h actual
4. Output: nuevo archivo listo para copiar al plugin
```

### FASE 4.5 — Expansion de parametros y diagnostico automatico

Despues de la optimizacion inicial (paso 3), el sistema ejecuta dos procesos adicionales antes de exportar a C++:

#### A) Diagnostico automatico de gaps timbricos

El script `03_optimize_params.py` generara un **reporte de diagnostico** por cada combinacion instrumento/genero. Cuando la distancia timbrica minima tras la optimizacion supera un umbral (>0.6 normalizado), significa que los parametros actuales NO pueden reproducir ese timbre y hay que expandir el sintetizador.

**Metricas del reporte:**

| Metrica | Significado |
|---------|-------------|
| `min_distance` | Distancia timbrica minima alcanzada (0=perfecto, >0.6=problema) |
| `worst_features` | Las 3 features con mayor error residual |
| `suggested_params` | Parametros DSP sugeridos para reducir el gap |
| `priority` | Alta/Media/Baja segun la magnitud del gap |

**Ejemplo de reporte:**

```json
{
  "instrument": "hihat",
  "genre": "trap",
  "min_distance": 0.87,
  "status": "GAP_DETECTADO",
  "worst_features": [
    { "feature": "spectral_centroid", "error": 0.42, "target": 8200, "achieved": 5100 },
    { "feature": "harmonic_ratio", "error": 0.31, "target": 0.15, "achieved": 0.52 },
    { "feature": "brightness_index", "error": 0.28, "target": 0.92, "achieved": 0.61 }
  ],
  "suggested_params": [
    "ringFreq — frecuencia de resonancia metalica (el centroid es muy bajo)",
    "metallicAmount — mezcla de partiales inarmonicos (harmonic_ratio demasiado alto)",
    "bandpassQ — Q del filtro para brillo selectivo (brightness insuficiente)"
  ],
  "priority": "ALTA"
}
```

**Output:** `Training/data/diagnostics/gap_report.json` — un archivo con todos los gaps detectados.

#### B) Expansion sistematica de parametros del sintetizador

Basandose en el reporte de diagnostico, se ampliaran los parametros de los instrumentos que tengan gaps. El objetivo es que cada instrumento tenga suficientes controles DSP para poder reproducir el timbre de los samples reales.

**Parametros actuales vs. parametros expandidos propuestos:**

| Instrumento | Params actuales | Params a anadir (si gap detectado) | Total estimado |
|-------------|----------------|--------------------------------------|----------------|
| **Kick** | 8 (subFreq, clickAmount, bodyDecay, pitchDrop, pitchDropTime, driveAmount, subLevel, tailLength) | subPhase, clickFreq, clickDecay, saturationCurve | 12 |
| **Snare** | 5 (bodyFreq, noiseAmount, bodyDecay, noiseDecay, snapAmount) | wireResonance, wireFreq, snappiness, noiseColor, bodyTone | 10 |
| **HiHat** | 2 (decay, tone) | ringFreq, metallicAmount, bandpassQ, noiseColor, numPartials, openness | 8 |
| **Clap** | 2 (decay, spread) | numLayers, layerSpacing, toneFreq, filterQ, roomSize | 7 |
| **Perc** | 3 (freq, decay, noiseMix) | bodyResonance, pitchEnvAmount, toneSharpness, harmonicContent | 7 |
| **808** | 4 (freq, sustain, distortion, slide) | subHarmonics, saturationCurve, slideShape, filterDrive, tailShape | 9 |
| **Lead** | 5 (freq, detune, pulseWidth, attack, decay) | vibratoRate, vibratoDepth, formantFreq, formantQ, waveshapeAmount | 10 |
| **Pluck** | 3 (freq, decay, brightness) | bodySize, stringDamping, pickPosition, harmonicStretch | 7 |
| **Pad** | 4 (freq, attack, release, detune) | morphRate, spectralSpread, chorusDepth, filterMovement, unisonVoices | 9 |
| **Texture** | 3 (density, brightness, movement) | grainSize, grainSpread, spectralFreeze, noiseType, modulationDepth | 8 |

**IMPORTANTE:** Solo se anadiran parametros donde el diagnostico detecte gaps reales. Si un instrumento ya reproduce bien los samples con sus parametros actuales, NO se toca.

#### C) Proceso iterativo completo

```
[Paso 1] Optimizar con parametros actuales (03_optimize_params.py)
    ↓
[Paso 2] Generar reporte de gaps (automatico en 03_optimize_params.py)
    ↓
[Paso 3] Si hay gaps ALTOS:
    ├─ Expandir synth_bridge.py con nuevos parametros
    ├─ Expandir SynthEngine C++ con el DSP correspondiente
    ├─ Actualizar bounds en 03_optimize_params.py
    └─ Re-ejecutar optimizacion (volver a Paso 1)
    ↓
[Paso 4] Si distancia < 0.4 para todos:
    └─ Proceder a exportar (04_export_genre_rules.py) y Capa 3
```

Este ciclo se repite hasta que todos los instrumentos/generos tengan distancia aceptable (<0.4). Tipicamente 1-3 iteraciones son suficientes.

#### D) Tabla de referencia: gap → solucion DSP

Si el analisis revela que el sintetizador no puede replicar ciertos aspectos:

| Gap detectado | Feature afectada | Solucion DSP |
|---------------|-----------------|--------------|
| Kick real tiene mas cuerpo sub-harmonico | warmth_index, fundamental_freq | Anadir segundo oscilador sub con fase independiente |
| Snare real tiene wire buzz mas complejo | spectral_flatness, noise_ratio | Anadir modelo de wire resonance (comb filter) |
| Lead real tiene formantes | mfcc_distance, spectral_centroid | Anadir banco de formantes (2-3 bandpass paralelos) |
| HiHat real tiene metallic ring complejo | harmonic_ratio, brightness_index | Anadir modelo de inharmonic partials (6-8 oscs) |
| Pad real tiene movimiento espectral lento | spectral_centroid std, pitch_contour | Anadir wavetable morphing o spectral freeze |
| Attack real tiene mas micro-variacion | transient_sharpness, crest_factor | Anadir transient shaper con noise burst modulado |
| 808 real tiene mas sub-armonicos | warmth_index, fundamental_freq | Anadir waveshaping con curva configurable |
| Pluck real tiene mas resonancia corporal | decay_time_ms, spectral_bandwidth | Anadir modelo Karplus-Strong con damping variable |
| Texture real tiene granularidad | zero_crossing_rate, spectral_flatness | Anadir motor granular basico (grainSize, spread) |
| Clap real tiene mas capas espaciales | stereo_width, transient_sharpness | Anadir multi-layer con spacing temporal variable |

Estos cambios se harian **solo donde los datos del diagnostico lo justifiquen**, no preventivamente.

### Resultado esperado
- Nuevo `GenreRules.h` con valores calibrados contra samples reales
- Parametros que reducen la distancia timbrica tipicamente un 40-70%
- Posibles nuevos parametros en los sintetizadores donde hagan falta

---

## CAPA 3 — ML Parameter Prediction

### Objetivo
Entrenar un modelo ligero que prediga parametros optimos para CUALQUIER combinacion de controles del usuario, no solo las 90 combinaciones base.

### Por que un modelo y no solo reglas mejoradas
Las reglas de Capa 2 dan valores fijos por genero. Pero el usuario tiene sliders continuos (brillo 0-100, cuerpo 0-100...). El modelo aprende la **interpolacion** entre perfiles de forma no-lineal.

### Dataset de entrenamiento

```
Input (7 valores):
  - instrument_id     (0..9, one-hot encoded → 10)
  - genre_id          (0..8, one-hot encoded → 9)
  - brillo            (0..1)
  - cuerpo            (0..1)
  - textura           (0..1)
  - movimiento        (0..1)
  - impacto           (0..1)
  Total: 24 features de entrada

Output (N valores):
  - Todos los parametros del instrumento correspondiente
  - Kick: subFreq, clickAmount, bodyDecay, pitchDrop, ... (~15 params)
  - Lead: oscDetune, pulseWidth, vibratoRate, ... (~18 params)
  - + 9 parametros de efectos
  Total: ~25-35 features de salida (depende del instrumento)
```

### Generacion del dataset

```
Para cada instrumento:
  Para cada genero:
    Para brillo in [0, 0.25, 0.5, 0.75, 1.0]:
      Para cuerpo in [0, 0.25, 0.5, 0.75, 1.0]:
        Para textura in [0, 0.25, 0.5, 0.75, 1.0]:
          Para movimiento in [0, 0.25, 0.5, 0.75, 1.0]:
            Para impacto in [0, 0.25, 0.5, 0.75, 1.0]:
              1. Generar audio con parametros de Capa 2 + modulacion
              2. Extraer features
              3. Optimizar parametros para minimizar distancia al perfil
              4. Guardar (input, output_params) como fila del dataset

Total: 10 × 9 × 5^5 = 281,250 filas (se puede reducir con sampling)
```

### Arquitectura del modelo

```
MLP (Multi-Layer Perceptron):

Input (24) → Dense(128, ReLU) → Dense(64, ReLU) → Dense(35, Sigmoid)

- 24 inputs (one-hot instrument + one-hot genre + 5 sliders)
- 128 neuronas capa 1
- 64 neuronas capa 2
- 35 outputs (parametros normalizados 0..1)
- Activacion final: Sigmoid (todos los params estan en 0..1 normalizado)

Tamano del modelo: ~15 KB en ONNX
Tiempo de inference: <0.1 ms
```

### Script: `05_train_model.py`

```python
import torch
import torch.nn as nn

class OneShotParamPredictor(nn.Module):
    def __init__(self, input_dim=24, hidden1=128, hidden2=64, output_dim=35):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(input_dim, hidden1),
            nn.ReLU(),
            nn.Dropout(0.1),
            nn.Linear(hidden1, hidden2),
            nn.ReLU(),
            nn.Dropout(0.1),
            nn.Linear(hidden2, output_dim),
            nn.Sigmoid()
        )

    def forward(self, x):
        return self.net(x)

# Training:
# - Loss: MSE entre parametros predichos y parametros optimales (Capa 2)
# - Optimizer: Adam, lr=1e-3
# - Epochs: 100-300
# - Validation: 80/20 split
# - Early stopping en val_loss
```

### Script: `06_export_onnx.py`

```python
# Exportar modelo entrenado a ONNX para cargarlo en C++
torch.onnx.export(model, dummy_input, "oneshot_param_predictor.onnx",
                  input_names=['controls'],
                  output_names=['params'],
                  opset_version=11)
```

### Integracion en C++ (MLParameterGenerator.h)

```
Plugin carga oneshot_param_predictor.onnx al iniciar (~15 KB)
    ↓
Usuario mueve sliders → GenerationRequest
    ↓
MLParameterGenerator::generate():
  1. Encode inputs (one-hot + sliders) → float[24]
  2. ONNX Runtime inference → float[35]
  3. Decode outputs → InstrumentParams + EffectChainParams
    ↓
SynthEngine::generate() (sin cambios)
    ↓
Audio de alta calidad calibrado contra samples reales
```

### Dependencia C++ para ONNX Runtime
```
// En el .jucer o CMake:
// onnxruntime-win-x64 (Microsoft, MIT license, ~15 MB DLL)
// Solo se necesita onnxruntime.dll + onnxruntime_c_api.h
```

---

## Orden de ejecucion

```
FASE 1: Setup                                              ✅ COMPLETADA
  ├─ Crear estructura Training/                             ✅
  ├─ 90 carpetas libraries/{inst}/{genero}/                 ✅
  ├─ 6 scripts + 4 utils escritos                           ✅
  ├─ venv Python 3.10 creado                                ✅
  └─ Todas las dependencias instaladas                      ✅

FASE 2: Cargar librerias (usuario)                         ⏳ PENDIENTE
  └─ Copiar samples .wav a Training/libraries/{inst}/{genero}/

FASE 3: Capa 1 — Feature Extraction                        ⏳ PENDIENTE
  ├─ python scripts/01_extract_features.py    → features por sample
  └─ python scripts/02_build_profiles.py --plots → perfiles + graficas

FASE 4: Capa 2 — Optimization                             ⏳ PENDIENTE
  ├─ Synth bridge Python ya implementado (Opcion B)
  ├─ python scripts/03_optimize_params.py --export-audio → params optimizados
  ├─ python scripts/04_export_genre_rules.py  → nuevo GenreRules.h
  └─ Integrar en plugin, testear, iterar

FASE 4.5: Diagnostico + Expansion de parametros            ⏳ PENDIENTE
  ├─ Analizar gap_report.json generado en FASE 4
  ├─ Si hay gaps ALTOS (distancia > 0.6):
  │   ├─ Expandir synth_bridge.py con nuevos parametros DSP
  │   ├─ Expandir SynthEngine C++ con DSP correspondiente
  │   ├─ Re-ejecutar 03_optimize_params.py (iterar hasta distancia < 0.4)
  │   └─ Re-exportar 04_export_genre_rules.py con params expandidos
  └─ Si todos OK (distancia < 0.4): proceder a FASE 5

FASE 5: Capa 3 — ML Model (opc1ional, despues de validar Capa 2 + 4.5)  ⏳ PENDIENTE
  ├─ python scripts/05_train_model.py         → modelo entrenado
  ├─ python scripts/06_export_onnx.py --verify → .onnx
  └─ Integrar MLParameterGenerator.h en plugin

FASE 6: Mejoras avanzadas de calidad y mutaciones              ⏳ PENDIENTE
  ├─ 6a. Multi-resolucion temporal en 01_extract_features.py
  ├─ 6b. Loss perceptual (mel, loudness, masking) en 03_optimize_params.py
  ├─ 6c. Ejes de mutacion PCA → nuevo 03b_learn_mutation_axes.py
  ├─ 6d. Quality scorer → nuevo 07_train_quality_scorer.py
  ├─ 6e. Perfiles de mutacion y humanizacion → modificar ParameterGenerator.h
  └─ 6f. Feature-space morphing → modificar 05_train_model.py
```

---

## Comandos rapidos

```bash
# Activar entorno
cd Training
venv\Scripts\activate

# Capa 1
python scripts/01_extract_features.py                       # todas las librerias
python scripts/01_extract_features.py --instrument kicks    # solo kicks
python scripts/01_extract_features.py --instrument kicks --genre trap  # solo kicks/trap
python scripts/02_build_profiles.py --plots                 # perfiles + graficas

# Capa 2
python scripts/03_optimize_params.py                        # optimizar todos
python scripts/03_optimize_params.py --instrument kicks --genre trap --maxiter 100
python scripts/03_optimize_params.py --export-audio         # + wav de ejemplo
python scripts/04_export_genre_rules.py                     # generar .h calibrado

# Capa 3
python scripts/05_train_model.py --epochs 200               # entrenar MLP
python scripts/05_train_model.py --generate-dataset-only    # solo generar dataset
python scripts/06_export_onnx.py --verify                   # exportar + verificar
```

---

## Requisitos minimos de librerias

| Instrumento | Samples minimos por genero | Ideal |
|-------------|---------------------------|-------|
| Kick        | 10                        | 30+   |
| Snare       | 10                        | 30+   |
| HiHat       | 10                        | 25+   |
| Clap        | 8                         | 20+   |
| Perc        | 8                         | 20+   |
| 808/Bass    | 8                         | 25+   |
| Lead        | 5                         | 15+   |
| Pluck       | 5                         | 15+   |
| Pad         | 5                         | 15+   |
| Texture     | 5                         | 15+   |

No es necesario tener todos los generos cubiertos desde el inicio. Se puede empezar con los mas importantes (Trap, Techno, House) y expandir.

---

## MEJORAS AVANZADAS — Calidad de sonido y mutaciones inteligentes

Estas mejoras se aplican tras completar las capas base (1-3). Cada una es independiente y se puede implementar por separado.

### 1. Loss function perceptual (psicoacustica)

**Problema actual:** La `timbral_distance()` usa pesos arbitrarios. Un error de 100Hz en el centroid no tiene el mismo impacto perceptual a 200Hz que a 8000Hz.

**Solucion:** Ponderar las distancias segun como percibe el oido humano:

```
Mejoras a la funcion de coste:

a) Escala Mel para frecuencias
   - Convertir spectral_centroid, fundamental_freq, rolloff a Mel antes de comparar
   - Un error de 50Hz a 100Hz es ENORME, a 8000Hz es imperceptible
   - mel(f) = 2595 * log10(1 + f/700)

b) Curvas de igual sonoridad (ISO 226)
   - Ponderar bandas frecuenciales segun sensibilidad del oido
   - El oido es mas sensible entre 1-5 kHz → errores ahi pesan mas
   - Menos sensible por debajo de 100Hz → el sub puede ser menos preciso

c) Enmascaramiento temporal
   - Si el ataque es fuerte, errores en los primeros 5ms importan mas
   - El decay es menos critico perceptualmente que el onset
   - Ponderar: attack_error * 3.0, decay_error * 1.0, tail_error * 0.5

d) Ponderacion por loudness
   - Features de componentes silenciosos importan menos
   - Si noise_ratio = 0.05, su precision importa menos que si es 0.4
```

**Impacto:** Los parametros optimizados se ajustaran a lo que *suena* diferente, no a lo que *mide* diferente. Mejora percibida estimada: 20-30%.

### 2. Analisis multi-resolucion temporal

**Problema actual:** Las features se extraen sobre el audio completo. Pero un kick tiene 3 fases totalmente distintas (click 0-5ms, body 5-80ms, tail 80-500ms) y cada una necesita parametros diferentes.

**Solucion:** Extraer features en ventanas temporales separadas:

```
Ventanas de analisis:

| Fase      | Ventana     | Features clave                           | Params que calibra             |
|-----------|-------------|------------------------------------------|--------------------------------|
| Transient | 0-5ms       | peak_slope, spectral_centroid, crest     | clickAmount, snapAmount        |
| Attack    | 0-20ms      | attack_shape, spectral_tilt, brightness  | pitchDrop, pitchDropTime       |
| Body      | 20-100ms    | fundamental, warmth, harmonic_ratio      | subFreq, bodyDecay, subLevel   |
| Sustain   | 100-300ms   | spectral_flatness, noise_ratio           | noiseAmount, filterCutoff      |
| Tail      | 300ms-end   | decay_curve, spectral_rolloff            | tailLength, release            |

Cada fase tiene su propia sub-distancia con pesos:
  total_distance = 0.30 * transient_dist
                 + 0.25 * attack_dist
                 + 0.25 * body_dist
                 + 0.10 * sustain_dist
                 + 0.10 * tail_dist
```

**Impacto:** Captura detalles que el analisis global pierde. Un kick puede tener el centroid correcto en promedio pero el click suena mal y el body bien. Con multi-resolucion se detecta y corrige.

### 3. Mutaciones correlacionadas (parameter coupling)

**Problema actual:** El `jitter()` muta cada parametro de forma independiente. Pero en sonidos reales, los parametros estan correlacionados: un kick con mas click tambien suele tener pitchDrop mas rapido, un snare con mas body tiene menos snap, etc.

**Solucion:** Aprender una **matriz de correlacion** de los parametros optimizados en Capa 2:

```
Proceso:

1. Tras optimizar los 90 sets de parametros, calcular la matriz de correlacion
   entre todos los parametros de cada instrumento

2. Identificar "ejes de variacion" con PCA:
   - Eje 1 del kick: "peso" (subFreq↑, bodyDecay↑, clickAmount↓, pitchDrop↓)
   - Eje 2 del kick: "punch" (clickAmount↑, pitchDrop↑, driveAmount↑)
   - Eje 3 del kick: "longitud" (tailLength↑, bodyDecay↑)

3. Las mutaciones se hacen a lo largo de estos ejes, no por parametro individual:
   - jitter_eje("peso", amount=0.1) → muta 4 parametros correlacionados a la vez
   - Resultado: mutaciones que suenan a "variaciones de un kick", no a "un kick roto"

4. Exportar ejes como "MutationAxes" al C++:
   mutation_axes_kick = {
     "peso":    { subFreq: +0.8, bodyDecay: +0.6, clickAmount: -0.4, pitchDrop: -0.3 },
     "punch":   { clickAmount: +0.9, pitchDrop: +0.7, driveAmount: +0.5 },
     "longitud": { tailLength: +0.9, bodyDecay: +0.5 }
   }
```

**Impacto:** Las mutaciones producen variaciones que suenan a "otro kick interesante" en vez de "un kick con un parametro raro". Mejora drastica en la coherencia de las variaciones.

### 4. Navegacion en el espacio de features (Feature-Space Morphing)

**Problema actual:** Cuando el usuario quiere un sonido "entre trap y techno", el sistema interpola parametros linealmente. Pero la interpolacion lineal de parametros no produce la interpolacion lineal del timbre (la relacion es no-lineal).

**Solucion:** Interpolar en el espacio de features, no de parametros:

```
Proceso:

1. Calcular el perfil de features de kick/trap y kick/techno
2. Interpolar: features_target = 0.5 * features_trap + 0.5 * features_techno
3. Optimizar parametros para que el audio generado se acerque a features_target
4. Resultado: un kick que SUENA a mitad de camino, no que tiene parametros a mitad

Aplicaciones:
  - Morph entre generos (slider continuo trap↔techno↔house)
  - Morph entre instrumentos (kick↔perc, snare↔clap)
  - "Direcciones timbricas": mas metalico, mas organico, mas oscuro, mas roto
  - El modelo MLP (Capa 3) aprende estas interpolaciones no-lineales automaticamente

Implementacion:
  - UMAP/t-SNE del espacio de features para visualizar el "mapa timbrico"
  - Identificar regiones "vacias" del mapa donde ningun genero vive → zonas creativas
  - Las mutaciones pueden explorar estas zonas vacias intencionalmente
```

**Impacto:** Abre un espacio creativo enorme. El usuario puede explorar sonidos que no existen en ningun genero concreto pero que son musicalmente coherentes.

### 5. Scoring de calidad perceptual

**Problema actual:** No hay forma de saber si un sonido generado "suena bien" o "suena a sintesis barata". Solo se mide distancia al target, no calidad intrinseca.

**Solucion:** Entrenar un segundo modelo ligero que puntue la calidad:

```
Quality Scorer (modelo secundario):

Training data:
  - Positivos (score=1.0): los samples reales de las librerias
  - Negativos (score=0.0): sonidos generados con parametros aleatorios
  - Intermedios (score=0.5): sonidos generados con parametros semi-optimizados

Modelo: MLP pequeno (21 features → 32 → 16 → 1 sigmoid)
Input: las 21 features de audio de un sonido generado
Output: score 0.0 (mala calidad) a 1.0 (calidad profesional)

Uso:
  1. En la optimizacion (Capa 2): anadir quality_score al coste total
     coste_total = timbral_distance * 0.7 + (1.0 - quality_score) * 0.3

  2. En las mutaciones: rechazar mutaciones con quality_score < 0.3
     Generar 5 variantes → puntuar → quedarse con las 2 mejores

  3. En el plugin: indicador visual de "calidad estimada" del sonido generado

Features mas predictivas de calidad:
  - Crest factor en rango natural (no demasiado comprimido ni demasiado debil)
  - Spectral balance (ni todo graves ni todo agudos)
  - Attack coherence (transiente limpio, no artefactos)
  - Harmonic structure (armonicos naturales, no aliasing)
```

**Impacto:** Actua como filtro de calidad. Elimina los sonidos "que miden bien pero suenan mal". Garantiza un minimo de calidad profesional en cada generacion.

### 6. Humanizacion y micro-variacion

**Problema actual:** Cada generacion con el mismo seed produce exactamente el mismo resultado. Suena "perfecto" pero "muerto" — le falta la imperfeccion que hace que un sonido suene real y vivo.

**Solucion:** Sistema de micro-variaciones controladas:

```
Capas de humanizacion (cada una se puede activar/desactivar):

a) Micro-timing del onset
   - Variacion de ±0.5ms en el inicio del transiente
   - Modela la imprecision natural de grabar percusion real
   - Parametro: humanize_timing (0.0=perfecto, 1.0=natural)

b) Micro-pitch drift
   - Variacion de ±2-5 cents en la frecuencia fundamental
   - Modela la inestabilidad natural de instrumentos acusticos
   - Especialmente efectivo en 808s, leads, pads
   - Parametro: humanize_pitch (0.0=perfecto, 1.0=natural)

c) Variacion timbrica por golpe
   - Cuando se generan multiples hits del mismo preset,
     aplicar micro-jitter (1-3%) en brightness, body, attack
   - Modela que cada golpe de caja es ligeramente diferente
   - Parametro: humanize_timbre (0.0=identico, 1.0=natural)

d) Modulacion de envolvente
   - Pequena variacion (±5%) en attack y decay entre hits
   - Modela la variacion natural de velocidad/fuerza al tocar
   - Parametro: humanize_dynamics (0.0=robotico, 1.0=natural)

Implementacion en C++:
  - Un solo parametro "Humanize" (0-100) en la UI que escala todos los sub-parametros
  - Bajo el capot: micro-jitter con distribucion gaussiana (no uniforme)
  - Correlacionado con velocity si viene de MIDI

Aprendizaje desde librerias:
  - Medir la variacion REAL entre samples del mismo instrumento/genero
  - Usar esa variacion como referencia para calibrar los rangos de humanizacion
  - Ejemplo: si los kicks de trap tienen std(attack)=0.8ms, usar eso como sigma
```

**Impacto:** Los sonidos generados dejan de sonar a "preset de sintetizador" y empiezan a sonar a "instrumento real grabado". Especialmente importante para percusion.

### 7. Perfiles de mutacion inteligentes

**Problema actual:** Todas las mutaciones usan la misma estrategia (jitter uniforme). Pero no es lo mismo querer "una variacion sutil" que "algo totalmente diferente".

**Solucion:** Definir perfiles de mutacion con diferentes intensidades y estrategias:

```
Perfiles de mutacion:

| Perfil       | Estrategia                                    | Uso tipico                    |
|--------------|-----------------------------------------------|-------------------------------|
| "subtle"     | Jitter 1-5% en 2-3 params, gaussiano          | Variaciones para un loop      |
| "moderate"   | Jitter 5-15% en 4-6 params, correlacionado    | Explorar dentro de un genero  |
| "creative"   | Jitter 15-40% en ejes PCA, cruce de generos   | Buscar sonidos nuevos         |
| "extreme"    | Jitter 30-80%, parametros independientes       | Sound design experimental     |
| "hybrid"     | Interpolar features de 2 instrumentos/generos  | Crear sonidos hibridos        |

Cada perfil tiene:
  - Que parametros muta (seleccion inteligente, no todos)
  - Cuanto muta (distribucion y rango)
  - Si usa correlaciones PCA o jitter independiente
  - Si aplica quality scoring como filtro

El usuario selecciona el perfil con el control "Mutation" existente:
  0-20:   subtle
  20-40:  moderate
  40-60:  creative
  60-80:  extreme
  80-100: hybrid (mezcla cross-genero/cross-instrumento)
```

**Impacto:** El slider de mutacion pasa de ser "mas o menos aleatorio" a ser "mas o menos creativo". Cada rango produce resultados musicalmente utiles en vez de ruido.

### Orden de implementacion de mejoras

```
Prioridad ALTA (mayor impacto en calidad):
  1. Multi-resolucion temporal          — mejora precision de optimizacion
  2. Mutaciones correlacionadas (PCA)   — mejora coherencia de variaciones
  3. Loss perceptual                    — mejora relevancia de la optimizacion

Prioridad MEDIA (mejora experiencia):
  4. Quality scoring                    — filtra resultados malos
  5. Perfiles de mutacion               — da control creativo al usuario
  6. Humanizacion                       — sonidos mas vivos

Prioridad BAJA (mejora exploratoria):
  7. Feature-space morphing             — abre espacio creativo avanzado

Todas estas mejoras se integran en los scripts existentes:
  - Mejoras 1, 3    → modificar 03_optimize_params.py (funcion de coste)
  - Mejora 2        → nuevo script 03b_learn_mutation_axes.py
  - Mejora 4        → nuevo script 07_train_quality_scorer.py
  - Mejoras 5, 6    → modificar ParameterGenerator.h (C++)
  - Mejora 7        → modificar 05_train_model.py (dataset con interpolaciones)
```

---

## Notas importantes

1. **No se toca el plugin hasta Capa 2 paso 4** — todo antes es Python offline
2. **Las librerias nunca se distribuyen con el plugin** — solo se usan para entrenar
3. **El plugin sigue funcionando sin modelo ML** — Capa 3 es un upgrade opcional
4. **Cada capa es independiente** — se puede parar en Capa 1 o 2 y ya hay mejora
5. **Seeds siguen funcionando** — la reproducibilidad se mantiene
6. **Los scripts generan visualizaciones** — para validar auditivamente cada paso
7. **Synth bridge usa Opcion B** (Python/NumPy) — los 10 instrumentos ya tienen generador basico en `synth_bridge.py`, se puede refinar o cambiar a Opcion A (CLI C++) si hace falta mas precision

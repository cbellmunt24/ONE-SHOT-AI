# PLAN DDSP — ONE-SHOT AI v2: Síntesis Neural con DDSP

## Visión general

Migrar ONE-SHOT AI de síntesis paramétrica (88 params + CMA-ES matching) a **DDSP (Differentiable Digital Signal Processing)** — síntesis neural basada en modelos sinusoidales diferenciables.

**Objetivo:** Máxima calidad profesional tanto en generación como en matching de one-shots.

**Filosofía:** DDSP = SMS (Sinusoidal Modeling Synthesis) + Red Neuronal entrenada end-to-end.

---

## Arquitectura final del plugin (3 modos)

```
┌─────────────────────────────────────────────────────────────┐
│                    ONE-SHOT AI v2                            │
│                                                             │
│  ┌──────────────────┐  ┌─────────────────┐  ┌───────────┐  │
│  │  GENERATOR MODE   │  │   MATCH MODE    │  │ SYNTH ED. │  │
│  │  (DDSP neural)    │  │  (DDSP encoder) │  │ (manual)  │  │
│  │                    │  │                 │  │           │  │
│  │ instrumento+género │  │  WAV referencia │  │ 88 knobs  │  │
│  │ + sliders          │  │       │         │  │     │     │  │
│  │       │            │  │       ▼         │  │     ▼     │  │
│  │       ▼            │  │  DDSP Encoder   │  │ Universal │  │
│  │  DDSP Decoder      │  │  (extract f0,   │  │  Synth    │  │
│  │  (ONNX Runtime)    │  │   loudness,     │  │ (C++ tal  │  │
│  │       │            │  │   timbre)       │  │  cual)    │  │
│  │       ▼            │  │       │         │  │           │  │
│  │  Harmonic amps     │  │       ▼         │  │           │  │
│  │  + f0 contour      │  │  DDSP Decoder   │  │           │  │
│  │  + noise filter    │  │  (ONNX Runtime) │  │           │  │
│  │  + transient env   │  │       │         │  │           │  │
│  │       │            │  │       ▼         │  │           │  │
│  │       ▼            │  │  Synth params   │  │           │  │
│  │  SMS Resíntesis    │  │       │         │  │           │  │
│  │  (C++ nativo)      │  │       ▼         │  │           │  │
│  │       │            │  │  SMS Resíntesis │  │           │  │
│  │       ▼            │  │  (C++ nativo)   │  │           │  │
│  │    AUDIO OUT       │  │       │         │  │           │  │
│  │                    │  │       ▼         │  │           │  │
│  │                    │  │    AUDIO OUT    │  │           │  │
│  └──────────────────┘  └─────────────────┘  └───────────┘  │
└─────────────────────────────────────────────────────────────┘
```

**Lo que se mantiene:** UniversalSynth (88 params) para el Synth Editor manual.
**Lo que cambia:** Generator y Match usan DDSP neural + SMS resíntesis.

---

## Decisiones técnicas clave

### ¿Por qué PyTorch y no el DDSP original de Google (TensorFlow)?

| | DDSP Google (TF) | PyTorch + ddsp-pytorch (IRCAM) |
|---|---|---|
| Framework | TensorFlow 2.x | PyTorch |
| Tu pipeline actual | Incompatible (PyTorch) | ✅ Compatible |
| Export a ONNX | TF→ONNX es problemático | PyTorch→ONNX nativo, 1 línea |
| Módulos de síntesis | Implementados y testeados | ✅ Implementados por IRCAM/ACIDS |
| Comunidad | Mantenido pero TF pierde adopción | PyTorch domina investigación audio |
| Debugging | TF es más opaco | PyTorch es Python nativo, fácil |

**Decisión:** Usaremos **`ddsp-pytorch` de IRCAM/ACIDS** como base (los módulos de síntesis diferenciable ya implementados en PyTorch), el repo de Google como **referencia conceptual/teórica**, y adaptaremos ambos para one-shots.

### ¿Qué descargar?

**1. `ddsp-pytorch` de IRCAM/ACIDS** — nuestra base de trabajo:
```
pip install ddsp-pytorch
# O clonar para modificar:
git clone https://github.com/acids-ircam/ddsp_pytorch.git
```
Esto nos da: HarmonicSynth, FilteredNoise, STFT loss, encoder/decoder — todo en PyTorch, testeado por investigadores de audio profesionales (IRCAM es el instituto de referencia mundial en audio computacional).

**2. DDSP de Google** — referencia conceptual:
```
git clone https://github.com/magenta/ddsp.git
```
Lo necesitamos para:
1. Estudiar la arquitectura original y los hiperparámetros del paper
2. Entender decisiones de diseño documentadas en el código
3. Referencia para la resíntesis aditiva + substractiva
4. **Futuro:** cuando amplíes a instrumentos monofónicos sostenidos, la teoría es directamente aplicable

### ¿Por qué no reimplementar desde cero?

Porque IRCAM ya lo hizo. `ddsp-pytorch` incluye:
- `harmonic_synth()` — banco de osciladores diferenciable
- `filtered_noise()` — ruido filtrado diferenciable
- Multi-resolution STFT loss
- Encoder/decoder base

No tiene sentido reimplementar módulos que investigadores de audio han testeado y publicado. Nosotros nos enfocamos en **adaptar para one-shots** (conditioning, transientes, duración variable) — que es donde está el valor añadido.

### Adaptaciones necesarias para one-shots

El DDSP original asume audio monofónico sostenido. Nuestros one-shots son diferentes:

| Aspecto | DDSP original | Nuestra adaptación |
|---|---|---|
| Duración | Segundos a minutos | 0.05s - 2s |
| Pitch | Continuo y estable | A veces inexistente (hihats, claps) |
| Estructura | Cuasi-estacionario | Transiente + decay |
| Training data | Minutos de 1 instrumento | ~5000 samples, 10 instrumentos |
| Conditioning | No (1 modelo = 1 instrumento) | Sí (instrumento + género) |

**Adaptaciones concretas:**
1. **Flag pitched/unpitched:** Para sonidos sin pitch (hihats, claps, percs), f0=0 y toda la energía va al componente de ruido filtrado
2. **Componente de transiente:** DDSP no modela clicks/snaps. Añadimos un tercer componente: transient envelope × impulse
3. **Modelo condicionado:** Un solo modelo que recibe one-hot de instrumento (10) + género (9) como conditioning
4. **Duración variable:** El decoder genera N frames adaptativos según la duración del sample
5. **Data augmentation:** Pitch shifting, time stretching, noise injection para multiplicar ×5 el dataset

---

## Fases de implementación

### FASE 0: Setup del entorno DDSP (Día 1)
**Objetivo:** Tener ddsp-pytorch de IRCAM + Google DDSP (referencia) + entorno listo

```
Pasos:
1. Crear entorno virtual dedicado (Python 3.10):
   cd "C:\Users\charl\Desktop\ESMUC\# SONOLOGIA ESMUC\LABSO II\ONE-SHOT AI\Training"
   python -m venv venv-ddsp
   venv-ddsp\Scripts\activate

2. Instalar PyTorch (con CUDA si tienes GPU NVIDIA):
   # CON GPU (recomendado):
   pip install torch torchaudio --index-url https://download.pytorch.org/whl/cu121
   # SIN GPU:
   pip install torch torchaudio

3. Instalar ddsp-pytorch de IRCAM/ACIDS (nuestra base):
   pip install ddsp-pytorch
   # También clonar para poder modificar si hace falta:
   git clone https://github.com/acids-ircam/ddsp_pytorch.git ddsp-ircam-reference

4. Clonar el repo de Google DDSP (referencia teórica):
   git clone https://github.com/magenta/ddsp.git ddsp-google-reference

5. Instalar dependencias adicionales:
   pip install librosa crepe scipy numpy matplotlib tensorboard onnx onnxruntime

6. Verificar instalación:
   python -c "import ddsp; print('ddsp-pytorch OK')"
   python -c "import torch; print(f'PyTorch {torch.__version__}, CUDA: {torch.cuda.is_available()}')"
   python -c "import torchaudio; print(f'torchaudio {torchaudio.__version__}')"
   python -c "import librosa; print(f'librosa {librosa.__version__}')"
```

**Estructura de directorios nueva:**
```
Training/
├── ddsp-google-reference/   ← repo clonado de Google (solo referencia teórica)
├── ddsp-ircam-reference/    ← repo clonado de IRCAM/ACIDS (referencia + base)
├── ddsp/                    ← NUESTRA adaptación para one-shots
│   ├── __init__.py
│   ├── core.py              ← importa/extiende ddsp-pytorch de IRCAM
│   ├── losses.py            ← multi-resolution STFT loss (de IRCAM + custom)
│   ├── encoders.py          ← audio → (f0, loudness, z_timbre) — adaptado one-shots
│   ├── decoders.py          ← (f0, loudness, z_timbre, cond) → synth params
│   ├── synth.py             ← modelo completo DDSPOneShotSynth
│   ├── transient.py         ← componente de transiente (NUEVO, no en DDSP original)
│   └── data.py              ← dataset loader para one-shots
├── scripts/
│   ├── 10_prepare_ddsp_dataset.py   ← analizar todos los WAVs → features
│   ├── 11_train_ddsp.py             ← entrenar modelo condicionado
│   ├── 12_export_ddsp_onnx.py       ← exportar a ONNX
│   ├── 13_evaluate_ddsp.py          ← métricas de calidad
│   └── ... (scripts antiguos se mantienen por referencia)
├── libraries/               ← tus ~5000 samples (sin cambios)
├── data/
│   ├── ddsp_dataset/        ← features pre-extraídas
│   ├── ddsp_models/         ← checkpoints de training
│   └── ddsp_onnx/           ← modelos exportados
└── venv-ddsp/               ← entorno virtual nuevo
```

---

### FASE 1: Preparación del dataset DDSP (Día 1-2)
**Objetivo:** Extraer features de todos los samples para training

**Script: `10_prepare_ddsp_dataset.py`**

Para cada WAV en `Training/libraries/`:
```
Input:  kick_trap_0001.wav (audio raw)
Output: kick_trap_0001.npz conteniendo:
  - audio: waveform normalizado (mono, 44100 Hz)
  - f0: pitch contour por frame (CREPE o YIN) — 0 si unpitched
  - loudness: A-weighted loudness por frame
  - is_pitched: bool (True para 808s, leads, plucks, pads; False para hihats, claps, etc.)
  - instrument_id: int (0-9)
  - genre_id: int (0-8)
  - duration: float (segundos)
  - n_frames: int (número de frames)
```

**Parámetros de análisis:**
- Sample rate: 44100 Hz (nativo)
- Frame size: 1024 samples (~23ms)
- Hop size: 256 samples (~5.8ms)
- f0 range: 20-12000 Hz (mismo que UniversalSynth)
- f0 method: CREPE (más robusto que YIN para pitch tracking — usa red neuronal)
- Loudness: A-weighted RMS en dB, normalizado 0-1

**Data augmentation (×5 el dataset):**
1. **Pitch shift:** ±2 semitonos (solo pitched sounds)
2. **Time stretch:** ±10% (preservando pitch)
3. **Gain variation:** ±3 dB
4. **Noise injection:** SNR 40dB (muy sutil)
5. **Combinaciones aleatorias** de los anteriores

**Clasificación pitched/unpitched:**
```python
PITCHED_INSTRUMENTS = {"808s", "leads", "plucks", "pads", "kicks"}  # kicks tienen pitch tonal
UNPITCHED_INSTRUMENTS = {"hihats", "claps", "percs", "snares", "textures"}
```
(Nota: algunos percs/textures pueden tener pitch — CREPE detectará si hay o no)

---

### FASE 2: Implementación del motor DDSP PyTorch (Día 2-4)
**Objetivo:** Adaptar ddsp-pytorch de IRCAM para one-shots + encoder/decoder condicionado

#### 2A. Módulos de síntesis diferenciable (`ddsp/core.py`)

**De IRCAM (ya implementados, importar directamente):**
```python
from ddsp.synths import Harmonic, FilteredNoise  # ← ya hechos por IRCAM
# Harmonic: banco de osciladores armónicos diferenciable
# FilteredNoise: ruido blanco + filtro FIR diferenciable
```

**Nuevo (implementar nosotros):**
```python
class TransientSynth(nn.Module):
    """Componente de transiente para clicks/snaps/attacks.
    NO está en DDSP original ni en ddsp-pytorch — es nuestra extensión
    para one-shots percusivos.
    
    Input:  transient_amps[T], transient_freqs[T], transient_decay[T]
    Output: audio[T * hop_size]
    
    Genera impulsos exponencialmente decayentes en posiciones específicas.
    """
```

El 80% del motor de síntesis viene de IRCAM testeado. Solo implementamos TransientSynth (para percusivos) y el wrapper que combina los tres componentes.

#### 2B. Encoder (`ddsp/encoders.py`)

```python
class OneShotEncoder(nn.Module):
    """Extrae representación latente de un one-shot.
    
    Input:  audio[samples] + instrument_id + genre_id
    Output: f0[T], loudness[T], z_timbre[T, latent_dim]
    
    - f0 y loudness vienen pre-computados (CREPE + A-weighted RMS)
    - z_timbre se extrae con un encoder temporal (GRU o 1D-CNN)
    - z_timbre codifica TODO lo que f0+loudness no capturan: timbre, texture, color
    """

    # Arquitectura:
    # audio → MelSpec → Conv1D stack → GRU → z_timbre[T, 16]
    #                                      (16 dims latentes por frame)
```

#### 2C. Decoder (`ddsp/decoders.py`)

```python
class OneShotDecoder(nn.Module):
    """Genera parámetros de síntesis desde features + conditioning.
    
    Input:  f0[T], loudness[T], z_timbre[T, 16],
            instrument_one_hot[10], genre_one_hot[9]
    Output: harmonic_amps[T, 64],      ← amplitud de 64 armónicos por frame
            noise_magnitudes[T, 65],    ← envolvente espectral del ruido (65 bandas)
            transient_amps[T],          ← amplitud de transiente por frame
            transient_params[3]         ← freq, decay, width del transiente
    
    Arquitectura:
      [f0, loudness, z_timbre, cond] → MLP(512, 512, 512) → split heads
    """
```

#### 2D. Modelo completo (`ddsp/synth.py`)

```python
class DDSPOneShotSynth(nn.Module):
    """Modelo completo: encoder → decoder → síntesis diferenciable.
    
    Training:  audio_real → encoder → decoder → synth → audio_pred → loss(real, pred)
    Inference: (instrument, genre, sliders) → decoder → synth → audio
    Match:     audio_ref → encoder → decoder → synth → audio_reconstructed
    """
    
    def __init__(self):
        self.encoder = OneShotEncoder()
        self.decoder = OneShotDecoder()
        self.harmonic_synth = HarmonicSynth(n_harmonics=64)
        self.noise_synth = FilteredNoise(n_freq_bands=65)
        self.transient_synth = TransientSynth()
    
    def forward(self, audio, f0, loudness, instrument_id, genre_id):
        # Encode
        z_timbre = self.encoder(audio, f0, loudness)
        
        # Decode
        h_amps, noise_mags, trans_amps, trans_params = self.decoder(
            f0, loudness, z_timbre, instrument_id, genre_id
        )
        
        # Synthesize (diferenciable!)
        harmonic_audio = self.harmonic_synth(f0, h_amps)
        noise_audio = self.noise_synth(noise_mags)
        transient_audio = self.transient_synth(trans_amps, trans_params)
        
        return harmonic_audio + noise_audio + transient_audio
```

#### 2E. Loss function (`ddsp/losses.py`)

```python
class DDSPLoss(nn.Module):
    """Multi-resolution STFT loss — estándar en DDSP.
    
    Compara spectrogramas a múltiples resoluciones (ventanas de 64 a 8192).
    Combina L1 en magnitud + L1 en log-magnitud.
    """
    
    # Resoluciones: [64, 128, 256, 512, 1024, 2048, 4096, 8192]
    # Para cada resolución:
    #   L_mag = L1(|STFT_pred|, |STFT_real|)
    #   L_log = L1(log|STFT_pred|, log|STFT_real|)
    #   L = L_mag + L_log
    # Loss total = sum(L) / N_resoluciones
```

---

### FASE 3: Training del modelo DDSP (Día 4-6)
**Objetivo:** Entrenar el modelo condicionado en tus ~5000 samples

**Script: `11_train_ddsp.py`**

**Hiperparámetros iniciales:**
```python
config = {
    "sample_rate": 44100,
    "hop_size": 256,
    "n_harmonics": 64,
    "n_noise_bands": 65,
    "latent_dim": 16,         # dims del z_timbre por frame
    "decoder_hidden": 512,
    "decoder_layers": 3,
    "n_instruments": 10,
    "n_genres": 9,
    "batch_size": 16,
    "learning_rate": 1e-3,
    "epochs": 500,
    "loss_weights": {
        "spectral": 1.0,       # multi-res STFT
        "perceptual": 0.1,     # mel-spectrogram L1
    },
}
```

**Estrategia de training:**
1. **Fase warmup (50 epochs):** Solo loss espectral, lr=1e-3
2. **Fase principal (300 epochs):** Loss espectral + perceptual, lr=1e-3 → 1e-5 (cosine)
3. **Fase fine-tune (150 epochs):** + loss de envelope temporal, lr=1e-5

**Data augmentation on-the-fly:**
- Random pitch shift ±2st (solo pitched)
- Random gain ±3dB
- Random trim (±5% de duración)

**Monitoring (TensorBoard):**
- Loss por resolución STFT
- Spectrogramas comparativos (real vs generado)
- Audio samples cada 50 epochs
- Loss separada por instrumento (detectar qué instrumentos cuestan más)

**Hardware estimado:**
- Con GPU (RTX 3060+): ~2-4 horas para 500 epochs
- Sin GPU (CPU): ~12-24 horas — viable pero lento
- Recomendación: si no tienes GPU potente, Google Colab gratis (T4)

---

### FASE 4: Evaluación y ajuste (Día 6-7)
**Objetivo:** Verificar calidad del modelo antes de integrarlo

**Script: `13_evaluate_ddsp.py`**

**Métricas:**
1. **Reconstruction quality (Match):**
   - Multi-resolution STFT distance
   - Mel cepstral distortion (MCD)
   - FAD (Fréchet Audio Distance) — estándar en generación de audio
   - Envelope RMSE
   - Listening test informal (A/B con originales)

2. **Generation quality:**
   - FAD entre generados y reales por instrumento×género
   - Diversidad: varianza intra-clase de las generaciones
   - Cobertura: ¿genera para todas las 90 combinaciones?

**Criterios de aceptación:**
- Reconstruction: MCD < 5 dB (excelente para one-shots)
- FAD < 10 (comparable a modelos publicados)
- Listening test: >80% de las reconstrucciones indistinguibles del original

**Si no cumple criterios:**
1. Aumentar capacity del decoder (512 → 1024)
2. Aumentar n_harmonics (64 → 128)
3. Más data augmentation
4. Entrenar modelos separados por "familia" (percusivos vs tonales)

---

### FASE 5: Export a ONNX (Día 7)
**Objetivo:** Modelo listo para C++

**Script: `12_export_ddsp_onnx.py`**

```python
# Exportar solo el decoder (el encoder solo se usa en match/training)
# Para Generator Mode: decoder + synth params → ONNX
# Para Match Mode: encoder + decoder → ONNX

# Modelo 1: ddsp_decoder.onnx
#   Input:  f0[T], loudness[T], z_timbre[T,16], instrument[10], genre[9]
#   Output: harmonic_amps[T,64], noise_mags[T,65], transient_amps[T], transient_params[3]

# Modelo 2: ddsp_encoder.onnx (para match mode)
#   Input:  mel_spec[T,N_mels], f0[T], loudness[T]
#   Output: z_timbre[T,16]

# Modelo 3: ddsp_generator.onnx (para generator mode — sin encoder)
#   Input:  instrument[10], genre[9], sliders[5], duration, seed
#   Output: f0[T], loudness[T], z_timbre[T,16]
#   (Este modelo aprende a GENERAR los features, no a extraerlos de audio)
```

**Verificación post-export:**
```python
# Comparar salida PyTorch vs ONNX Runtime
# Máxima diferencia aceptable: < 1e-5
```

---

### FASE 6: Integración C++ en el plugin (Día 7-10)
**Objetivo:** SMS resíntesis nativa + carga ONNX en JUCE

#### 6A. Nuevo módulo `Source/DDSP/`

```
Source/DDSP/
├── DDSPParams.h           ← structs para harmonic_amps, noise_mags, etc.
├── DDSPInference.h        ← wrapper ONNX Runtime (cargar modelos, ejecutar)
├── HarmonicSynth.h        ← banco de 64 osciladores sinusoidales (C++ puro)
├── FilteredNoiseSynth.h   ← ruido blanco + filtro FIR variable
├── TransientSynth.h       ← impulsos + decay
├── DDSPEngine.h           ← orquestador: inference → synthesis → audio
└── DDSPFeatureExtractor.h ← extraer f0 + loudness en C++ (para match mode)
```

#### 6B. HarmonicSynth.h — El componente clave

```cpp
// Banco de osciladores armónicos — resíntesis SMS
class HarmonicSynth
{
public:
    // Genera audio desde trayectorias de armónicos
    // f0[nFrames], amps[nFrames][nHarmonics] → audio[nSamples]
    juce::AudioBuffer<float> render (
        const std::vector<float>& f0,           // freq fundamental por frame
        const std::vector<std::array<float, 64>>& amps,  // 64 armónicos por frame
        float sampleRate, int hopSize)
    {
        // Para cada sample:
        //   1. Interpolar f0 y amps desde el frame anterior al actual
        //   2. Para cada armónico: phase += f0 * n / sr; sample += sin(phase) * amp
        //   3. Sumar todos los armónicos
    }

private:
    std::array<double, 64> phases {};  // fase acumulada por armónico
};
```

#### 6C. Flujo en el plugin

```
GENERATOR MODE:
  UI (instrumento, género, sliders)
  → DDSPInference::generate(instrument, genre, sliders)
    → ejecuta ddsp_generator.onnx → f0, loudness, z_timbre
    → ejecuta ddsp_decoder.onnx → harmonic_amps, noise_mags, transient
  → HarmonicSynth::render() + FilteredNoiseSynth::render() + TransientSynth::render()
  → mix → post-process → AudioBuffer

MATCH MODE:
  UI (cargar WAV)
  → DDSPFeatureExtractor::analyze(wav) → f0, loudness, mel_spec
  → DDSPInference::encode(mel_spec, f0, loudness) → z_timbre
  → DDSPInference::decode(f0, loudness, z_timbre) → harmonic_amps, noise_mags, transient
  → HarmonicSynth::render() + FilteredNoiseSynth::render() + TransientSynth::render()
  → mix → post-process → AudioBuffer

SYNTH EDITOR MODE:
  (sin cambios — UniversalSynth con 88 knobs)
```

---

### FASE 7: Actualizar WebUI (Día 10-11)
**Objetivo:** Adaptar la interfaz a los nuevos modos

**Cambios en Match tab (`OneShotMatchWebUI.h`):**
- Eliminar: progress bar de CMA-ES (ya no hay optimización iterativa)
- Eliminar: blend slider de residual compensation (ya no hace falta)
- Añadir: visualización de parciales (spectrograma con tracks)
- Mantener: botón load WAV, botón play, score display
- Match ahora es **instantáneo** (~100ms vs ~30s del CMA-ES)

**Cambios en Generator tab (`OneShotWebUI.h`):**
- Mantener: selector instrumento × género
- Mantener: 5 sliders (brillo, cuerpo, textura, movimiento, impacto)
- Cambiar: backend de UniversalSynth a DDSP
- Añadir: slider de "variación" (ruido en z_timbre para diversidad)

**Nuevo en Synth Editor tab (`OneShotSynthWebUI.h`):**
- Mantener: 88 knobs del UniversalSynth (modo manual legacy)
- Añadir: botón "Load from DDSP" (importar el último resultado DDSP como punto de partida)

---

### FASE 8: Testing y polish (Día 11-13)
**Objetivo:** Verificar todo funciona end-to-end

1. **Compilar Release** y verificar que no hay crashes
2. **Test Generator:** Generar las 90 combinaciones, escuchar todas
3. **Test Match:** Cargar 10 samples representativos, verificar reconstrucción
4. **Test Synth Editor:** Verificar que UniversalSynth sigue funcionando
5. **Test rendimiento:** Medir tiempo de generación y match (objetivo: <200ms)
6. **A/B listening test:** Comparar generaciones DDSP vs UniversalSynth antiguo

---

## Resumen de archivos que cambian

### Archivos NUEVOS (crear)
```
Training/ddsp/                    ← módulo Python DDSP completo (7 archivos)
Training/scripts/10_*.py          ← preparar dataset
Training/scripts/11_*.py          ← entrenar
Training/scripts/12_*.py          ← exportar ONNX
Training/scripts/13_*.py          ← evaluar
Source/DDSP/                      ← módulo C++ DDSP completo (6 archivos)
```

### Archivos que se MODIFICAN
```
Source/WebUI/OneShotMatchWebUI.h  ← simplificar (quitar CMA-ES progress, blend)
Source/WebUI/OneShotWebUI.h       ← cambiar backend de generación
Source/WebViewPluginDemo.h        ← nuevos API endpoints para DDSP
ESTADO_PLUGIN.md                  ← actualizar estado
CLAUDE.md                         ← actualizar arquitectura
```

### Archivos que se MANTIENEN sin cambios
```
Source/UniversalSynth/*           ← todo intacto (Synth Editor lo usa)
Source/DSP/*                      ← todo intacto
Source/Effects/*                  ← todo intacto
Source/Params/*                   ← todo intacto
Source/AI/*                       ← mantener (legacy, referencia)
Training/scripts/01-07_*.py       ← mantener (pipeline antiguo como referencia)
Training/libraries/*              ← sin cambios (son los samples)
```

### Archivos que se podrían ELIMINAR (no urgente)
```
Source/OneShotMatch/OneShotMatchOptimizer.h  ← CMA-ES ya no se usa
Source/OneShotMatch/OneShotMatchEngine.h     ← reemplazado por DDSPEngine
(Recomendación: no borrar aún, mantener como referencia hasta que DDSP funcione)
```

---

## Dependencias nuevas

### Python (training)
```
torch >= 2.0
torchaudio >= 2.0
ddsp-pytorch (IRCAM/ACIDS) — pip install ddsp-pytorch  ← módulos de síntesis diferenciable
librosa >= 0.10
crepe (pitch tracking neuronal) — pip install crepe
tensorboard
onnx
onnxruntime
scipy
numpy
matplotlib
```

### C++ (plugin)
```
ONNX Runtime — ya lo usas (verificar versión >= 1.15)
(No se necesita nada más — la resíntesis SMS es C++ puro)
```

---

## Cronograma estimado

| Fase | Descripción | Días | Acumulado |
|------|-------------|------|-----------|
| 0 | Setup entorno | 1 | 1 |
| 1 | Preparar dataset | 1-2 | 2-3 |
| 2 | Implementar DDSP PyTorch | 2-3 | 4-6 |
| 3 | Entrenar modelo | 2 | 6-8 |
| 4 | Evaluar y ajustar | 1 | 7-9 |
| 5 | Export ONNX | 0.5 | 7.5-9.5 |
| 6 | Integración C++ | 3 | 10.5-12.5 |
| 7 | WebUI | 1 | 11.5-13.5 |
| 8 | Testing | 2 | 13.5-15.5 |

**Total estimado: ~2 semanas de trabajo**

---

## Riesgos y mitigaciones

| Riesgo | Probabilidad | Mitigación |
|--------|-------------|------------|
| Dataset demasiado pequeño | Media | Data augmentation ×5 + transfer learning |
| Sonidos unpitched reconstruyen mal | Media | Componente de transiente + más bandas de ruido |
| ONNX export falla | Baja | PyTorch→ONNX es nativo y fiable. Tracing vs scripting si hace falta |
| Rendimiento C++ lento | Baja | 64 osciladores × 44100 = ~2.8M ops/s, trivial para CPU moderna |
| Modelo no generaliza entre géneros | Media | Más conditioning, embeddings aprendidos vs one-hot |
| ddsp-pytorch incompatible | Baja | Si IRCAM da problemas, reimplementamos core (son ~200 líneas) |

---

## Notas finales

- **No borrar nada del sistema actual** hasta que DDSP funcione completamente
- **El UniversalSynth se queda** como modo manual y como fallback
- **Los scripts de training antiguos se quedan** como referencia
- **`ddsp-pytorch` de IRCAM/ACIDS es nuestra base** — módulos de síntesis diferenciable ya hechos
- **DDSP de Google se clona como referencia teórica** — no como dependencia directa
- **PyTorch es nuestro framework**, ONNX es nuestro formato de deploy
- **Futuro:** cuando amplíes a instrumentos monofónicos reales, la arquitectura DDSP escala directamente

Este plan transforma ONE-SHOT AI de un plugin con synth paramétrico + optimización bruta
a un plugin con **síntesis neural de calidad profesional** comparable a herramientas como
Alchemy, DDSP-VST, o los motores internos de Splice/Output.

# Kick Match System — Technical Documentation

## 1. Purpose

Kick Match is a sound reconstruction system that analyzes a reference kick drum sample and reconstructs it using internal synthesis parameters. Unlike the Generator mode which creates new sounds from ML-predicted parameters, Kick Match works backwards: it starts with a target sound and finds the synthesis parameters that best reproduce it.

The goal is to validate that our synthesis engine is capable of recreating professional-quality kick drums through parameter optimization.

## 2. Why It Exists Alongside the Generator

The plugin now serves two complementary purposes:

- **Generator Mode** (existing): Creates novel one-shot samples using an ML model trained on a large dataset. The model predicts synthesis parameters for any instrument/genre combination. This is a *generative* system.

- **Kick Match Mode** (new): Takes an existing reference sample and finds synthesis parameters that reconstruct it. This is an *analysis-by-synthesis* system.

Both systems share the same DSP primitives (oscillators, filters, envelopes, saturation) but use them for different purposes. Kick Match validates synthesis quality against real-world targets, while the Generator creates new sounds from learned distributions.

## 3. Plugin Architecture

```
PLUGIN (WebViewPluginAudioProcessorWrapper)
│
├── WebViewPluginAudioProcessor
│   ├── ParameterGenerator (ML/rule-based)    ← Generator Mode
│   ├── SynthEngine (10 instruments)          ← Generator Mode
│   └── KickMatchEngine                       ← Kick Match Mode
│       ├── DescriptorExtractor
│       ├── KickMatchSynth
│       └── KickMatchOptimizer
│
├── WebUI (HTML/CSS/JS served via resource provider)
│   ├── OneShotWebUI (Generator tab)
│   └── KickMatchWebUI (Kick Match tab)
│
└── API endpoints
    ├── /api/generate          ← Generator
    ├── /api/export            ← Generator
    ├── /api/kickmatch/load    ← Kick Match
    ├── /api/kickmatch/analyze ← Kick Match
    ├── /api/kickmatch/start   ← Kick Match
    ├── /api/kickmatch/status  ← Kick Match
    ├── /api/kickmatch/cancel  ← Kick Match
    ├── /api/kickmatch/audio   ← Kick Match
    ├── /api/kickmatch/export-audio  ← Kick Match
    └── /api/kickmatch/export-preset ← Kick Match
```

### File Structure

```
Source/
├── KickMatch/                          ← NEW MODULE
│   ├── KickMatchDescriptors.h          # Audio descriptor extraction & distance
│   ├── KickMatchSynth.h               # Dedicated kick synthesizer
│   ├── KickMatchOptimizer.h           # DE + Nelder-Mead optimization
│   └── KickMatchEngine.h              # Top-level orchestrator
│
├── WebUI/
│   ├── OneShotWebUI.h                 # Generator UI (now with tab system)
│   └── KickMatchWebUI.h              # Kick Match UI (NEW)
│
├── WebViewPluginDemo.h                # Processor + API endpoints (extended)
│
└── [existing directories unchanged]
    ├── AI/
    ├── DSP/
    ├── Effects/
    ├── Params/
    ├── SynthEngine/
    └── Test/
```

## 4. Analysis-by-Synthesis Workflow

```
┌──────────────────────────────────────────────────────┐
│  1. User drops reference kick sample                  │
│                                                       │
│  2. Extract descriptors from reference                │
│     → KickDescriptors (temporal, pitch, spectral)    │
│                                                       │
│  3. Initialize optimizer with population              │
│     → First individual seeded from reference features │
│     → Remaining individuals random within bounds      │
│                                                       │
│  4. FOR each iteration:                               │
│     a. Generate kick from candidate parameters        │
│     b. Extract descriptors from generated kick        │
│     c. Compute distance(ref, generated)               │
│     d. Update population (Differential Evolution)     │
│     e. Report progress to UI                          │
│                                                       │
│  5. Local refinement (Nelder-Mead) on best solution  │
│                                                       │
│  6. Output:                                           │
│     → Matched kick audio                              │
│     → Optimized synthesis parameters                  │
│     → Descriptor similarity score                     │
└──────────────────────────────────────────────────────┘
```

## 5. Audio Descriptors

The system extracts and compares these descriptors:

### Temporal
| Descriptor | Unit | Description |
|---|---|---|
| Attack Time | seconds | Time to reach peak amplitude |
| Decay Time | seconds | Time from peak to -20 dB |
| Transient Strength | ratio | Peak / RMS (crest factor) |

### Pitch
| Descriptor | Unit | Description |
|---|---|---|
| Fundamental Frequency | Hz | Estimated F0 of the body (30-100ms post-peak) |
| Pitch Start | Hz | Frequency at transient onset |
| Pitch End | Hz | Frequency at body steady-state |
| Pitch Drop Time | seconds | Time for frequency to settle to fundamental |

### Spectral
| Descriptor | Unit | Description |
|---|---|---|
| Spectral Centroid | Hz | Center of mass of the spectrum |
| Spectral Rolloff | Hz | Frequency below which 85% of energy exists |
| Harmonic/Noise Ratio | ratio | Energy at harmonics of F0 vs total energy |

### Perceptual
| Descriptor | Unit | Description |
|---|---|---|
| RMS Loudness | linear | Root mean square of the entire sample |
| Brightness | ratio | Energy above 2kHz / total energy |

**Extraction method**: Temporal descriptors use amplitude envelope analysis. Pitch estimation uses zero-crossing rate on windowed segments. Spectral descriptors use a 2048-point FFT with Hann windowing.

## 6. Optimization Loop

### Phase 1: Differential Evolution (Global Search)

- **Population size**: 30 individuals
- **Mutation factor (F)**: 0.7 (reduces to 0.4 in later iterations)
- **Crossover rate (CR)**: 0.8 (increases to 0.9 in later iterations)
- **Max iterations**: 100
- **Strategy**: DE/rand/1/bin

The first individual is seeded with an intelligent guess derived from the reference descriptors (fundamental frequency, decay time, pitch drop time, transient strength). This dramatically accelerates convergence.

### Phase 2: Nelder-Mead (Local Refinement)

After DE converges, Nelder-Mead simplex method runs 50 additional iterations around the best solution found by DE. This fine-tunes continuous parameters within a 5% neighborhood.

### Distance Function

Weighted Euclidean distance across normalized descriptors:

```
distance = Σ wᵢ × ((ref_i - gen_i) / scale_i)²
```

Weights are tuned for trap kick characteristics:
- Fundamental frequency: **3.0** (most important)
- Spectral centroid: **2.0**
- Decay time: **2.0**
- Attack time: **1.5**
- Pitch start: **1.5**
- Brightness: **1.5**
- Transient strength: **0.8**

**Convergence target**: distance < 0.5

## 7. Synthesis Parameters

The Kick Match synthesizer exposes 13 parameters:

| Parameter | Range | Description |
|---|---|---|
| `oscType` | 0-2 | Body oscillator: sine, triangle, saw |
| `basePitch` | 25-120 Hz | Fundamental frequency |
| `pitchEnvDepth` | 0-72 st | Pitch drop from transient |
| `pitchEnvDecay` | 0.001-0.2 s | Speed of pitch drop |
| `ampAttack` | 0.0001-0.01 s | Attack time |
| `ampDecay` | 0.05-1.0 s | Body decay time |
| `ampTail` | 0.05-1.5 s | Sub tail ring-out |
| `clickAmount` | 0-1 | Transient click intensity |
| `clickFreq` | 1000-12000 Hz | Click BP filter center |
| `distortion` | 0-1 | Tape saturation amount |
| `noiseAmount` | 0-1 | Noise layer level |
| `filterCutoff` | 200-20000 Hz | Main lowpass cutoff |
| `subLevel` | 0-1 | Sub oscillator mix |

### Synthesis Architecture

```
Body oscillator (sine/tri/saw)
    → Multi-stage pitch envelope (fast + slow exponential)
    → Body amplitude envelope (punch + sustain)
    ↓
Sub oscillator (sine at fundamental)
    → Lowpass filter
    → Sub envelope
    ↓
Click layer (filtered pink noise)
    → Bandpass filter at clickFreq
    → Fast exponential decay
    ↓
                MIX
                 ↓
        Tape saturation
                 ↓
         Lowpass filter
                 ↓
          DC blocker
                 ↓
         Normalize + fade-out
```

## 8. Trap Kick Reconstruction — Phase 1 Focus

Typical trap kick characteristics that the system targets:

- **Strong transient click**: 1-5ms pink noise burst, bandpass filtered around 3-5kHz
- **Sine-wave body**: Pure sine at 40-60 Hz with fast pitch drop
- **Fast pitch drop**: 24-60 semitones in 10-60ms
- **Short amplitude envelope**: 150-400ms total duration
- **Mild saturation**: Tape-style warmth, not aggressive distortion

The optimizer is biased toward these characteristics through:
1. Initial guess seeded from reference analysis
2. Parameter bounds tuned for kick drum ranges
3. Distance weights emphasizing fundamental frequency and spectral shape

## 9. Long-Term Expansion Plan

### Phase 2: Other Drum Categories
- Snare reconstruction (body + noise + wire components)
- Clap reconstruction (layered transients + tail)
- Hi-hat reconstruction (metallic + noise balance)
- Percussion reconstruction (tonal + inharmonic)

Each category will require:
- Dedicated synthesizer with appropriate parameters
- Potentially different descriptors (e.g., metallicity for hi-hats)
- Category-specific distance weights

### Phase 3: Cross-Type Transformation
- Morph between matched presets
- Transform one drum type's characteristics onto another
- "Make this snare sound like it belongs with this kick"

### Phase 4: Generator Integration
- Use Kick Match results to calibrate the Generator
- Feedback loop: match → extract parameters → improve ML training data
- Controlled variation around matched presets

## API Reference

All endpoints use the JUCE WebBrowserComponent resource provider (GET requests).

| Endpoint | Response | Description |
|---|---|---|
| `GET /api/kickmatch/load?path=<filepath>` | JSON | Load reference audio from file path |
| `GET /api/kickmatch/analyze` | JSON | Extract descriptors from loaded reference |
| `GET /api/kickmatch/start` | JSON | Start the optimization (background thread) |
| `GET /api/kickmatch/status` | JSON | Poll matching progress and state |
| `GET /api/kickmatch/cancel` | JSON | Cancel running optimization |
| `GET /api/kickmatch/audio` | audio/wav | Get the matched kick as WAV |
| `GET /api/kickmatch/export-audio` | JSON | Export matched kick to Desktop |
| `GET /api/kickmatch/export-preset` | JSON | Export synth parameters as JSON preset |

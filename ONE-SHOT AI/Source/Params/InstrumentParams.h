#pragma once

#include "BaseSoundParams.h"

// ============================================================
// DRUMS
// ============================================================

struct KickParams
{
    BaseSoundParams base;

    float subFreq       = 50.0f;    // Hz, frecuencia fundamental del sub
    float clickAmount   = 0.3f;     // 0..1, transiente de ataque
    float bodyDecay     = 0.25f;    // segundos, duración del cuerpo
    float pitchDrop     = 48.0f;    // semitonos de caída desde el transiente
    float pitchDropTime = 0.04f;    // segundos, velocidad de caída de pitch
    float driveAmount   = 0.0f;     // 0..1, distorsión/saturación
    float subLevel      = 0.8f;     // 0..1, nivel del componente sub
    float tailLength    = 0.3f;     // segundos
};

struct SnareParams
{
    BaseSoundParams base;

    float bodyFreq      = 200.0f;   // Hz, tono del cuerpo
    float bodyDecay     = 0.12f;    // segundos
    float noiseDecay    = 0.15f;    // segundos, duración del ruido
    float noiseColor    = 0.5f;     // 0..1 (0=oscuro, 1=brillante)
    float snapAmount    = 0.5f;     // 0..1, transiente de ataque
    float ringFreq      = 350.0f;   // Hz, frecuencia del ring/timbre
    float ringAmount    = 0.2f;     // 0..1
    float wireAmount    = 0.3f;     // 0..1, bordonera simulada
    float noiseTightness = 0.5f;    // 0..1, Q del filtro de ruido (más alto = menos hissy)
    float bodyMix       = 0.30f;    // 0..1, presencia del body tonal vs noise
    float noiseLP       = 8000.0f;  // Hz, LP en el noise para cortar hiss
};

struct HiHatParams
{
    BaseSoundParams base;

    float freqRange     = 8000.0f;  // Hz, frecuencia central metálica
    float metallic      = 0.7f;     // 0..1, carácter metálico
    float noiseColor    = 0.7f;     // 0..1 (0=oscuro, 1=brillante)
    float openDecay     = 0.4f;     // segundos, decay abierto
    float closedDecay   = 0.02f;    // segundos, decay cerrado
    float openAmount    = 0.0f;     // 0..1 (0=cerrado, 1=abierto)
    float ringAmount    = 0.3f;     // 0..1, resonancia metálica
    float highPassFreq  = 4000.0f;  // Hz
};

struct ClapParams
{
    BaseSoundParams base;

    float numLayers     = 4.0f;     // 1..8, capas de impacto
    float layerSpacing  = 0.015f;   // segundos, separación entre capas
    float noiseDecay    = 0.12f;    // segundos
    float noiseColor    = 0.5f;     // 0..1
    float toneAmount    = 0.2f;     // 0..1, componente tonal
    float toneFreq      = 1000.0f;  // Hz
    float reverbAmount  = 0.3f;     // 0..1, cola de reverb corta
    float thickness     = 0.5f;     // 0..1, grosor percibido
    float noiseTightness = 0.5f;    // 0..1, Q del BP (más alto = menos hissy, más crack)
    float noiseLP       = 7000.0f;  // Hz, LP post-BP para eliminar hiss agudo
    float transientSnap = 0.6f;     // 0..1, definición del transiente (crack vs spread)
};

struct PercParams
{
    BaseSoundParams base;

    float freq          = 400.0f;   // Hz, frecuencia tonal principal
    float toneDecay     = 0.08f;    // segundos
    float metallic      = 0.0f;     // 0..1, carácter metálico
    float woodiness     = 0.0f;     // 0..1, carácter de madera
    float membrane      = 0.0f;     // 0..1, carácter de membrana
    float clickAmount   = 0.4f;     // 0..1, transiente
    float pitchDrop     = 12.0f;    // semitonos
    float harmonicRatio = 1.0f;     // ratio de armónicos (1.0 = harmónico, >1 = inarmónico)
};

// ============================================================
// BASS / SYNTHS
// ============================================================

struct Bass808Params
{
    BaseSoundParams base;

    float subFreq       = 40.0f;    // Hz
    float sustainLevel  = 0.7f;     // 0..1
    float glideTime     = 0.0f;     // segundos, portamento
    float glideAmount   = 0.0f;     // semitonos
    float saturation    = 0.3f;     // 0..1
    float subOctave     = 0.0f;     // 0..1, mezcla de sub-octava
    float harmonics     = 0.2f;     // 0..1, contenido armónico
    float tailLength    = 1.5f;     // segundos
    float reeseMix      = 0.0f;     // 0..1, 0=pure 808 sine, 1=Reese (detuned saws)
    float reeseDetune   = 0.15f;    // 0..1, detune entre saws para Reese
    float punchiness    = 0.3f;     // 0..1, énfasis del transiente de ataque
    float filterEnvAmt  = 0.0f;     // 0..1, envolvente de filtro (para donk)
};

struct LeadParams
{
    BaseSoundParams base;

    float oscDetune     = 0.1f;     // 0..1, desafinacion entre osciladores
    float pulseWidth    = 0.5f;     // 0..1, ancho de pulso (PWM)
    float vibratoRate   = 5.0f;     // Hz
    float vibratoDepth  = 0.0f;     // 0..1
    float portamento    = 0.0f;     // segundos
    float brightness    = 0.5f;     // 0..1, apertura de filtro
    float waveformMix   = 0.0f;     // 0..1 (0=saw, 1=square con PWM)
    float unisonVoices  = 1.0f;     // 1..8
    float filterEnvAmt  = 0.0f;     // 0..1, envolvente de filtro (abre/cierra cutoff)
    float subOscLevel   = 0.0f;     // 0..1, nivel del sub-oscillator (sine -1 octava)
};

struct PluckParams
{
    BaseSoundParams base;

    float brightness    = 0.6f;     // 0..1, brillo inicial
    float bodyResonance = 0.3f;     // 0..1, resonancia del cuerpo
    float decayTime     = 0.4f;     // segundos
    float damping       = 0.5f;     // 0..1, amortiguamiento de agudos
    float pickPosition  = 0.5f;     // 0..1, posición de pulsación
    float stringTension = 0.5f;     // 0..1, tensión de cuerda simulada
    float harmonics     = 0.3f;     // 0..1, contenido armónico
    float stereoWidth   = 0.3f;     // 0..1
};

struct PadParams
{
    BaseSoundParams base;

    float unisonVoices  = 4.0f;     // 1..16, numero de voces unison
    float detuneSpread  = 0.2f;     // 0..1, spread de desafinacion
    float driftRate     = 0.3f;     // Hz, velocidad de drift analogico
    float warmth        = 0.5f;     // 0..1, caracter calido (cutoff warmth LP)
    float evolutionRate = 0.1f;     // Hz, velocidad de evolucion timbrica
    float filterSweep   = 0.0f;     // 0..1, barrido de filtro
    float stereoWidth   = 0.7f;     // 0..1
    float chorusAmount  = 0.3f;     // 0..1
    float airAmount     = 0.15f;    // 0..1, capa de ruido filtrado (respira)
    float subLevel      = 0.3f;     // 0..1, nivel del sub sine (peso/profundidad)
};

// ============================================================
// TEXTURAS / FX
// ============================================================

struct TextureParams
{
    BaseSoundParams base;

    float density       = 0.5f;     // 0..1, densidad granular o de capas
    float grainSize     = 0.05f;    // segundos, tamaño de grano
    float scatter       = 0.3f;     // 0..1, dispersión temporal
    float spectralTilt  = 0.0f;     // -1..1 (negativo=oscuro, positivo=brillante)
    float movement      = 0.5f;     // 0..1, movimiento interno
    float noiseColor    = 0.5f;     // 0..1
    float stereoWidth   = 0.8f;     // 0..1
    float evolutionRate = 0.2f;     // Hz, velocidad de cambio
};

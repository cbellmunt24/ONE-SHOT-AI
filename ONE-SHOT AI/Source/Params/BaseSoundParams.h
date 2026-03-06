#pragma once

#include "SynthEnums.h"

// Parámetros base compartidos por todos los instrumentos
struct BaseSoundParams
{
    // Output
    float volume    = 0.8f;     // 0..1
    float pan       = 0.5f;     // 0..1 (0=L, 0.5=C, 1=R)

    // Oscilador
    OscillatorType oscType = OscillatorType::Sine;

    // Envelope ADSR
    float attack    = 0.001f;   // segundos
    float decay     = 0.3f;     // segundos
    float sustain   = 0.0f;     // 0..1
    float release   = 0.1f;     // segundos

    // Pitch
    float pitchBase             = 60.0f;    // MIDI note (60 = C4)
    float pitchEnvelopeAmount   = 0.0f;     // semitonos de modulación
    float pitchDecay            = 0.05f;    // segundos

    // Filtro
    FilterType filterType       = FilterType::LowPass;
    float filterCutoff          = 20000.0f; // Hz
    float filterResonance       = 0.0f;     // 0..1

    // Carácter
    float saturationAmount  = 0.0f;     // 0..1
    float noiseAmount       = 0.0f;     // 0..1

    // LFO
    float lfoRate           = 0.0f;     // Hz
    float lfoDepth          = 0.0f;     // 0..1
    LfoDestination lfoDest  = LfoDestination::None;
};

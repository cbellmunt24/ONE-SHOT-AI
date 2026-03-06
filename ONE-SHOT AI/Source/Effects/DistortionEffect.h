#pragma once

#include <JuceHeader.h>
#include "EffectParams.h"
#include "../DSP/Saturator.h"
#include "../DSP/SynthFilter.h"
#include "../DSP/Oversampling.h"

// Distortion — UPGRADED multi-mode con 2x oversampling
//
// Mejoras:
//   — Multi-mode saturation (Tape, Tube, SoftClip) via Saturator
//   — 2x oversampling para reducir aliasing de armonicos
//   — Tone filter post-distorsion mejorado
//   — DC blocker post-saturacion

class DistortionEffect
{
public:
    void process (juce::AudioBuffer<float>& buffer, const DistortionParams& p, float sampleRate)
    {
        if (! p.enabled || p.mix < 0.001f)
            return;

        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        const float wet = p.mix;
        const float dry = 1.0f - wet;

        // Drive normalizado a rango util del Saturator
        float drive = p.drive * 0.9f + 0.05f; // 0.05 .. 0.95

        // Seleccionar modo de saturacion segun drive
        // Bajo drive = Tape (warmth), medio = Tube (caracter), alto = SoftClip (agresivo)
        dsputil::SaturationMode mode;
        if (drive < 0.35f)
            mode = dsputil::SaturationMode::Tape;
        else if (drive < 0.7f)
            mode = dsputil::SaturationMode::Tube;
        else
            mode = dsputil::SaturationMode::SoftClip;

        // Tone filter: LP desde 2kHz (dark) a 16kHz (bright)
        float toneFreq = 2000.0f + p.tone * 14000.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            dsputil::Saturator saturator;
            dsputil::OversamplingProcessor oversampler;
            oversampler.prepare (sampleRate);

            dsputil::SVFilter toneFilter;
            toneFilter.setParameters (toneFreq, 0.15f, FilterType::LowPass, sampleRate);

            // DC blocker post-saturacion
            float dcX1 = 0.0f, dcY1 = 0.0f;
            const float dcCoeff = 0.9986f;

            auto* data = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                float in = data[i];

                // 2x oversampled saturation (reduce aliasing armonicos)
                float distorted = oversampler.process ([&] (float x)
                {
                    return saturator.process (x, drive, mode);
                }, in);

                // DC block
                float dcOut = distorted - dcX1 + dcCoeff * dcY1;
                dcX1 = distorted;
                dcY1 = dcOut;
                distorted = dcOut;

                // Tone filter post-distorsion
                distorted = toneFilter.process (distorted);

                data[i] = in * dry + distorted * wet;
            }
        }
    }
};

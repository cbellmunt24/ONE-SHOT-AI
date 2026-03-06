#pragma once

#include <JuceHeader.h>
#include "EffectParams.h"
#include "../DSP/DSPConstants.h"

// Phaser: cadena de filtros allpass de primer orden con LFO.
// Crea notches que se mueven por el espectro.

class PhaserEffect
{
public:
    void process (juce::AudioBuffer<float>& buffer, const PhaserParams& p, float sampleRate)
    {
        if (! p.enabled || p.mix < 0.001f)
            return;

        const int numSamples = buffer.getNumSamples();
        const float wet = p.mix;
        const float dry = 1.0f - wet;
        const int stages = std::max (2, std::min (8, p.stages));
        const float feedback = std::min (p.feedback, 0.95f);

        // Allpass state per channel (max 8 stages)
        float apStateL[8] = {};
        float apStateR[8] = {};
        float lastOutL = 0.0f;
        float lastOutR = 0.0f;

        auto* dataL = buffer.getWritePointer (0);
        auto* dataR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            float t = (float) i / sampleRate;

            // LFO modula la frecuencia central de los allpass
            float lfo = std::sin (dsputil::TWO_PI * p.rate * t);
            // Rango: 200Hz .. 4000Hz
            float centerFreq = 200.0f + (1.0f + lfo * p.depth) * 1900.0f;

            // Coeficiente allpass desde la frecuencia
            float halfW = dsputil::PI * centerFreq / sampleRate;
            float tanHW = std::tan (halfW);
            float coeff = (1.0f - tanHW) / (1.0f + tanHW);

            // Process L — first-order allpass: y = coeff*(x - y_prev) + x_prev
            float inL = dataL[i] + lastOutL * feedback;
            float sampleL = inL;
            for (int s = 0; s < stages; ++s)
            {
                float y = coeff * sampleL + apStateL[s];
                apStateL[s] = sampleL - coeff * y;
                sampleL = y;
            }
            lastOutL = sampleL;

            // Process R
            if (dataR)
            {
                float inR = dataR[i] + lastOutR * feedback;
                float sampleR = inR;
                for (int s = 0; s < stages; ++s)
                {
                    float y = coeff * sampleR + apStateR[s];
                    apStateR[s] = sampleR - coeff * y;
                    sampleR = y;
                }
                lastOutR = sampleR;
                dataR[i] = dataR[i] * dry + sampleR * wet;
            }

            dataL[i] = dataL[i] * dry + sampleL * wet;
        }
    }
};

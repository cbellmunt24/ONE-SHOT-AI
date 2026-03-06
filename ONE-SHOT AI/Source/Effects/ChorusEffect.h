#pragma once

#include <JuceHeader.h>
#include "EffectParams.h"
#include "../DSP/DelayLine.h"
#include "../DSP/DSPConstants.h"

// Chorus — UPGRADED a 3 voces con Hermite interpolation
//
// 3 voces con LFOs desfasados 120 grados
// Hermite interpolation en delays modulados (sin artefactos HF)
// Feedback sutil para riqueza

class ChorusEffect
{
public:
    void process (juce::AudioBuffer<float>& buffer, const ChorusParams& p, float sampleRate)
    {
        if (! p.enabled || p.mix < 0.001f)
            return;

        const int numSamples = buffer.getNumSamples();
        const float wet = p.mix;
        const float dry = 1.0f - wet;

        const float baseDelayMs = 7.0f;
        const float modDepthMs = p.depth * 3.5f;
        int maxDelaySamples = (int)((baseDelayMs + modDepthMs + 2.0f) * 0.001f * sampleRate) + 4;

        // 3 voces por canal
        dsputil::DelayLine delayL[3], delayR[3];
        for (int v = 0; v < 3; ++v)
        {
            delayL[v].setSize (maxDelaySamples);
            delayR[v].setSize (maxDelaySamples);
            delayL[v].clear();
            delayR[v].clear();
        }

        // Phase offsets: 0, 120, 240 grados
        static constexpr float phaseOffsets[3] = { 0.0f, 2.094f, 4.189f };
        // Rates ligeramente diferentes por voz para riqueza
        static constexpr float rateMultipliers[3] = { 1.0f, 1.07f, 0.93f };

        // Feedback
        const float feedback = p.depth * 0.15f;

        auto* dataL = buffer.getWritePointer (0);
        auto* dataR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

        float lastOutL[3] = {}, lastOutR[3] = {};

        for (int i = 0; i < numSamples; ++i)
        {
            float inL = dataL[i];
            float inR = dataR ? dataR[i] : inL;

            float t = (float) i / sampleRate;

            float chorusL = 0.0f, chorusR = 0.0f;

            for (int v = 0; v < 3; ++v)
            {
                float rate = p.rate * rateMultipliers[v];

                // LFO con fases desfasadas + offset L/R
                float lfoL = std::sin (dsputil::TWO_PI * rate * t + phaseOffsets[v]);
                float lfoR = std::sin (dsputil::TWO_PI * rate * t + phaseOffsets[v] + dsputil::PI * 0.5f);

                float delayMsL = baseDelayMs + lfoL * modDepthMs;
                float delayMsR = baseDelayMs + lfoR * modDepthMs;

                float delaySamplesL = delayMsL * 0.001f * sampleRate;
                float delaySamplesR = delayMsR * 0.001f * sampleRate;

                // Write con feedback sutil
                delayL[v].write (inL + lastOutL[v] * feedback);
                delayR[v].write (inR + lastOutR[v] * feedback);

                // Hermite interpolation (sin artefactos HF)
                float outL = delayL[v].readHermite (delaySamplesL);
                float outR = delayR[v].readHermite (delaySamplesR);

                lastOutL[v] = outL;
                lastOutR[v] = outR;

                chorusL += outL;
                chorusR += outR;
            }

            // Normalizar 3 voces
            chorusL *= 0.333f;
            chorusR *= 0.333f;

            dataL[i] = inL * dry + chorusL * wet;
            if (dataR)
                dataR[i] = inR * dry + chorusR * wet;
        }
    }
};

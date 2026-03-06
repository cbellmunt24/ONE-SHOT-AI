#pragma once

#include <JuceHeader.h>
#include "EffectParams.h"
#include "../DSP/SynthFilter.h"

// Compresor feed-forward — UPGRADED
//
// Mejoras:
//   — Sidechain HP filter a ~100Hz (evita pumping por bass)
//   — Log-domain gain smoothing (mas natural)
//   — Gain smoothing mejorado

class CompressorEffect
{
public:
    void process (juce::AudioBuffer<float>& buffer, const CompressorParams& p, float sampleRate)
    {
        if (! p.enabled)
            return;

        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        const float wet = p.mix;
        const float dry = 1.0f - wet;

        // Envelope coefficients
        float attackCoeff  = std::exp (-1.0f / (p.attackMs * 0.001f * sampleRate));
        float releaseCoeff = std::exp (-1.0f / (p.releaseMs * 0.001f * sampleRate));

        float threshLin = std::pow (10.0f, p.threshold / 20.0f);
        float ratio = std::max (1.0f, p.ratio);
        float makeupLin = std::pow (10.0f, p.makeupGain / 20.0f);

        // Sidechain HP filter a ~100Hz (evita que el bass cause pumping)
        dsputil::OnePole sidechainHP;
        sidechainHP.setHighPass (100.0f, sampleRate);

        float envLevel = 0.0f;

        // Gain smoothing
        float smoothedGain = 1.0f;
        float gainSmoothCoeff = std::exp (-1.0f / (0.002f * sampleRate)); // ~2ms

        for (int i = 0; i < numSamples; ++i)
        {
            // Detector: max abs across channels (con sidechain HP)
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float raw = buffer.getSample (ch, i);
                float filtered = sidechainHP.process (raw);
                float absVal = std::abs (filtered);
                if (absVal > peak) peak = absVal;
            }

            // Envelope follower (ballistics)
            if (peak > envLevel)
                envLevel = attackCoeff * envLevel + (1.0f - attackCoeff) * peak;
            else
                envLevel = releaseCoeff * envLevel + (1.0f - releaseCoeff) * peak;

            // Gain computation in log domain
            float gain = 1.0f;
            if (envLevel > threshLin && envLevel > 0.0001f)
            {
                float envDb = 20.0f * std::log10 (envLevel);
                float threshDb = p.threshold;
                float overDb = envDb - threshDb;
                float compressedDb = threshDb + overDb / ratio;
                float targetDb = compressedDb - envDb;
                gain = std::pow (10.0f, targetDb / 20.0f);
            }

            gain *= makeupLin;

            // Smooth gain changes (prevents clicks)
            smoothedGain = gain + gainSmoothCoeff * (smoothedGain - gain);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float in = buffer.getSample (ch, i);
                float compressed = in * smoothedGain;
                buffer.setSample (ch, i, in * dry + compressed * wet);
            }
        }
    }
};

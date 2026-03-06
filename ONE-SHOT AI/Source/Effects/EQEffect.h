#pragma once

#include <JuceHeader.h>
#include "EffectParams.h"
#include "../DSP/DSPConstants.h"

// EQ de 3 bandas: low shelf, parametric mid, high shelf.
// Implementación biquad directa.

class EQEffect
{
public:
    void process (juce::AudioBuffer<float>& buffer, const EQParams& p, float sampleRate)
    {
        if (! p.enabled)
            return;

        // Skip if all gains are ~0
        if (std::abs (p.lowGain) < 0.1f && std::abs (p.midGain) < 0.1f && std::abs (p.highGain) < 0.1f)
            return;

        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        // Biquad coefficients
        struct BiquadCoeffs { float b0, b1, b2, a1, a2; };
        struct BiquadState  { float x1 = 0, x2 = 0, y1 = 0, y2 = 0; };

        auto calcLowShelf = [] (float freq, float gainDb, float sr) -> BiquadCoeffs
        {
            if (std::abs (gainDb) < 0.1f)
                return { 1, 0, 0, 0, 0 };
            float A = std::pow (10.0f, gainDb / 40.0f);
            float w0 = dsputil::TWO_PI * freq / sr;
            float cosw = std::cos (w0);
            float sinw = std::sin (w0);
            float alpha = sinw / 2.0f * std::sqrt (2.0f);
            float sqA = std::sqrt (A);

            float b0 = A * ((A + 1) - (A - 1) * cosw + 2 * sqA * alpha);
            float b1 = 2 * A * ((A - 1) - (A + 1) * cosw);
            float b2 = A * ((A + 1) - (A - 1) * cosw - 2 * sqA * alpha);
            float a0 = (A + 1) + (A - 1) * cosw + 2 * sqA * alpha;
            float a1 = -2 * ((A - 1) + (A + 1) * cosw);
            float a2 = (A + 1) + (A - 1) * cosw - 2 * sqA * alpha;

            float inv = 1.0f / a0;
            return { b0 * inv, b1 * inv, b2 * inv, a1 * inv, a2 * inv };
        };

        auto calcHighShelf = [] (float freq, float gainDb, float sr) -> BiquadCoeffs
        {
            if (std::abs (gainDb) < 0.1f)
                return { 1, 0, 0, 0, 0 };
            float A = std::pow (10.0f, gainDb / 40.0f);
            float w0 = dsputil::TWO_PI * freq / sr;
            float cosw = std::cos (w0);
            float sinw = std::sin (w0);
            float alpha = sinw / 2.0f * std::sqrt (2.0f);
            float sqA = std::sqrt (A);

            float b0 = A * ((A + 1) + (A - 1) * cosw + 2 * sqA * alpha);
            float b1 = -2 * A * ((A - 1) + (A + 1) * cosw);
            float b2 = A * ((A + 1) + (A - 1) * cosw - 2 * sqA * alpha);
            float a0 = (A + 1) - (A - 1) * cosw + 2 * sqA * alpha;
            float a1 = 2 * ((A - 1) - (A + 1) * cosw);
            float a2 = (A + 1) - (A - 1) * cosw - 2 * sqA * alpha;

            float inv = 1.0f / a0;
            return { b0 * inv, b1 * inv, b2 * inv, a1 * inv, a2 * inv };
        };

        auto calcPeaking = [] (float freq, float gainDb, float Q, float sr) -> BiquadCoeffs
        {
            if (std::abs (gainDb) < 0.1f)
                return { 1, 0, 0, 0, 0 };
            float A = std::pow (10.0f, gainDb / 40.0f);
            float w0 = dsputil::TWO_PI * freq / sr;
            float cosw = std::cos (w0);
            float sinw = std::sin (w0);
            float alpha = sinw / (2.0f * Q);

            float b0 = 1 + alpha * A;
            float b1 = -2 * cosw;
            float b2 = 1 - alpha * A;
            float a0 = 1 + alpha / A;
            float a1 = -2 * cosw;
            float a2 = 1 - alpha / A;

            float inv = 1.0f / a0;
            return { b0 * inv, b1 * inv, b2 * inv, a1 * inv, a2 * inv };
        };

        BiquadCoeffs lowCoeffs  = calcLowShelf (p.lowFreq, p.lowGain, sampleRate);
        BiquadCoeffs midCoeffs  = calcPeaking (p.midFreq, p.midGain, p.midQ, sampleRate);
        BiquadCoeffs highCoeffs = calcHighShelf (p.highFreq, p.highGain, sampleRate);

        // Process per channel
        for (int ch = 0; ch < numChannels; ++ch)
        {
            BiquadState lowState, midState, highState;
            auto* data = buffer.getWritePointer (ch);

            for (int i = 0; i < numSamples; ++i)
            {
                float x = data[i];

                // Low shelf
                auto processBiquad = [] (float x, const BiquadCoeffs& c, BiquadState& s) -> float
                {
                    float y = c.b0 * x + c.b1 * s.x1 + c.b2 * s.x2 - c.a1 * s.y1 - c.a2 * s.y2;
                    s.x2 = s.x1; s.x1 = x;
                    s.y2 = s.y1; s.y1 = y;
                    return y;
                };

                x = processBiquad (x, lowCoeffs, lowState);
                x = processBiquad (x, midCoeffs, midState);
                x = processBiquad (x, highCoeffs, highState);

                data[i] = x;
            }
        }
    }
};

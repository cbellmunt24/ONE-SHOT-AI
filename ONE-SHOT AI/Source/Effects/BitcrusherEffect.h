#pragma once

#include <JuceHeader.h>
#include "EffectParams.h"

// Bitcrusher: reducción de bit depth y sample rate.

class BitcrusherEffect
{
public:
    void process (juce::AudioBuffer<float>& buffer, const BitcrusherParams& p, float sampleRate)
    {
        if (! p.enabled || p.mix < 0.001f)
            return;

        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        const float wet = p.mix;
        const float dry = 1.0f - wet;

        // Bit depth quantization
        float bits = std::max (1.0f, std::min (16.0f, p.bitDepth));
        float levels = std::pow (2.0f, bits);

        // Sample rate reduction
        float srFactor = std::max (1.0f, p.sampleRateReduction);
        int holdSamples = (int) srFactor;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            float holdValue = 0.0f;
            int holdCounter = 0;

            for (int i = 0; i < numSamples; ++i)
            {
                float in = data[i];

                // Sample rate reduction (sample & hold)
                if (holdCounter <= 0)
                {
                    holdValue = in;
                    holdCounter = holdSamples;
                }
                holdCounter--;

                // Bit depth reduction
                float crushed = std::round (holdValue * levels) / levels;

                data[i] = in * dry + crushed * wet;
            }
        }
    }
};

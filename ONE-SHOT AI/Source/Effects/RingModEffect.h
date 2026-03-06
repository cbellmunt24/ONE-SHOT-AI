#pragma once

#include <JuceHeader.h>
#include "EffectParams.h"
#include "../DSP/DSPConstants.h"

// Ring modulator: multiplica la señal por un oscilador sinusoidal.

class RingModEffect
{
public:
    void process (juce::AudioBuffer<float>& buffer, const RingModParams& p, float sampleRate)
    {
        if (! p.enabled || p.mix < 0.001f)
            return;

        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        const float wet = p.mix;
        const float dry = 1.0f - wet;

        for (int i = 0; i < numSamples; ++i)
        {
            float t = (float) i / sampleRate;
            float mod = std::sin (dsputil::TWO_PI * p.frequency * t);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float in = buffer.getSample (ch, i);
                float ringMod = in * mod;
                buffer.setSample (ch, i, in * dry + ringMod * wet);
            }
        }
    }
};

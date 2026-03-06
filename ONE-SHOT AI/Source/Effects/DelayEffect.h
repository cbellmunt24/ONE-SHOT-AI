#pragma once

#include <JuceHeader.h>
#include "EffectParams.h"
#include "../DSP/DelayLine.h"
#include "../DSP/SynthFilter.h"

// Delay estéreo con feedback y filtro high-cut en el loop.
// Modo stereo = ping-pong (L→R→L).

class DelayEffect
{
public:
    void process (juce::AudioBuffer<float>& buffer, const DelayParams& p, float sampleRate)
    {
        if (! p.enabled || p.mix < 0.001f)
            return;

        const int numSamples = buffer.getNumSamples();
        const float wet = p.mix;
        const float dry = 1.0f - wet;

        int delaySamples = std::max (1, (int)(p.time * sampleRate));

        dsputil::DelayLine delayL, delayR;
        delayL.setSize (delaySamples + 1);
        delayR.setSize (delaySamples + 1);
        delayL.clear();
        delayR.clear();

        // High-cut filter en feedback loop
        dsputil::SVFilter hcL, hcR;
        hcL.setParameters (p.highCut, 0.1f, FilterType::LowPass, sampleRate);
        hcR.setParameters (p.highCut, 0.1f, FilterType::LowPass, sampleRate);

        float feedback = std::min (p.feedback, 0.95f); // safety limit

        auto* dataL = buffer.getWritePointer (0);
        auto* dataR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

        float fbL = 0.0f, fbR = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float inL = dataL[i];
            float inR = dataR ? dataR[i] : inL;

            float delayedL = delayL.read (delaySamples);
            float delayedR = delayR.read (delaySamples);

            // Filter feedback
            float filteredL = hcL.process (delayedL);
            float filteredR = hcR.process (delayedR);

            if (p.stereo)
            {
                // Ping-pong: L feedback goes to R, R goes to L
                delayL.write (inL + filteredR * feedback);
                delayR.write (inR + filteredL * feedback);
            }
            else
            {
                delayL.write (inL + filteredL * feedback);
                delayR.write (inR + filteredR * feedback);
            }

            dataL[i] = inL * dry + delayedL * wet;
            if (dataR)
                dataR[i] = inR * dry + delayedR * wet;
        }
    }
};

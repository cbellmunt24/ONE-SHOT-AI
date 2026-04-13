#pragma once

#include <JuceHeader.h>
#include "../Params/SynthParams.h"
#include "../Effects/EffectChain.h"
#include "../UniversalSynth/UniversalSynth.h"

// Motor de sintesis principal — UniversalSynth backend
// Orden: generate → effects → DC block → soft clip → limiter → normalize

class SynthEngine
{
public:
    juce::AudioBuffer<float> generate (const GenerationResult& result,
                                       double sampleRate,
                                       unsigned int seed = 0)
    {
        if (seed == 0)
        {
            std::random_device rd;
            seed = rd();
        }

        // Clamp critical params to safe ranges
        auto params = result.universalParams;
        params.duration = std::max (0.01f, std::min (params.duration, 5.0f));
        params.basePitch = std::max (20.0f, std::min (params.basePitch, 12000.0f));
        params.masterGain = std::max (0.0f, std::min (params.masterGain, 1.0f));
        auto buffer = universalSynth.generate (params, sampleRate, seed);

        if (buffer.getNumSamples() == 0)
        {
            buffer.setSize (2, (int) (sampleRate * 0.3));
            buffer.clear();
            return buffer;
        }

        effectChain.process (buffer, result.effects, (float) sampleRate);

        synthutil::dcBlockBuffer (buffer);
        synthutil::softClipBuffer (buffer, 1.08f);
        synthutil::truePeakLimiter (buffer, (float) sampleRate, -0.3f);
        synthutil::normalizeBuffer (buffer, 0.91f);

        return buffer;
    }

private:
    universalsynth::UniversalSynth universalSynth;
    EffectChain effectChain;
};

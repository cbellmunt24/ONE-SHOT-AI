#pragma once

#include <JuceHeader.h>
#include "../Params/InstrumentParams.h"
#include "../DSP/Oscillator.h"
#include "../DSP/NoiseGenerator.h"
#include "../DSP/SynthFilter.h"
#include "../DSP/Saturator.h"
#include "../DSP/DCBlocker.h"
#include "SynthUtils.h"

// Percusion generica profesional — UPGRADED con pink noise
//
// Mejoras:
//   — Pink noise para madera/membrana (mas natural)
//   — Noise coloreado segun woodiness/membrane

class PercSynth
{
public:
    juce::AudioBuffer<float> generate (const PercParams& p, double sampleRate, unsigned int seed)
    {
        const float sr = (float) sampleRate;

        float duration = p.toneDecay * 3.0f + 0.1f;
        int numSamples = synthutil::durationInSamples (duration, sampleRate);

        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        dsputil::NoiseGenerator noise;
        noise.setSeed (seed);

        double tonePhase = 0.0;
        double metallicPhase = 0.0;

        dsputil::SVFilter noiseFilter;
        float nfFreq = std::min (p.freq * 1.5f, sr * 0.48f);
        float nfQ = 0.2f + p.woodiness * 0.4f + p.membrane * 0.3f;
        noiseFilter.setParameters (nfFreq, nfQ, FilterType::BandPass, sr);

        dsputil::SVFilter clickBP;
        clickBP.setParameters (3000.0f + p.clickAmount * 3000.0f, 0.15f,
                               FilterType::BandPass, sr);

        dsputil::SVFilter mainFilter;
        mainFilter.setParameters (p.base.filterCutoff, p.base.filterResonance,
                                  p.base.filterType, sr);

        float startFreq = p.freq * std::pow (2.0f, p.pitchDrop / 12.0f);

        dsputil::DCBlocker dcBlock;

        for (int i = 0; i < numSamples; ++i)
        {
            float t = (float) i / sr;

            // === PITCH ENVELOPE ===
            float pitchEnv = std::exp (-t / std::max (0.001f, p.toneDecay * 0.2f));
            float currentFreq = p.freq + (startFreq - p.freq) * pitchEnv;

            // === MAIN TONE (fundamental + 2do armónico para cuerpo) ===
            tonePhase += (double) currentFreq / (double) sr;
            if (tonePhase >= 1.0) tonePhase -= 1.0;
            float tone = std::sin ((float)(tonePhase * dsputil::TWO_PI)) * 0.8f
                       + std::sin ((float)(tonePhase * dsputil::TWO_PI * 2.0)) * 0.2f;

            // === METALLIC PARTIAL ===
            float metallicTone = 0.0f;
            if (p.metallic > 0.01f)
            {
                float metalFreq = std::min (currentFreq * p.harmonicRatio, sr * 0.48f);
                metallicPhase += (double) metalFreq / (double) sr;
                if (metallicPhase >= 1.0) metallicPhase -= 1.0;
                metallicTone = std::sin ((float)(metallicPhase * dsputil::TWO_PI))
                             * p.metallic * 0.4f;
            }

            float toneEnv = std::exp (-t / std::max (0.001f, p.toneDecay));

            // === NOISE: pink para woodiness/membrane, white para metallic ===
            float noiseColor = p.metallic; // mas metalico = mas white
            float noiseRaw = noise.nextColored (0.3f + noiseColor * 0.7f);
            float noiseShaped = noiseFilter.process (noiseRaw);
            float noiseAmount = p.woodiness + p.membrane;
            float noiseEnv = std::exp (-t / std::max (0.001f, p.toneDecay * 0.6f));

            // === CLICK ===
            float clickEnv = std::exp (-t / 0.0005f);
            float click = clickBP.process (noise.next()) * clickEnv * p.clickAmount * 1.2f;

            // === MIX ===
            float sample = (tone + metallicTone) * toneEnv * 0.55f
                         + noiseShaped * noiseEnv * noiseAmount * 0.35f
                         + click;

            sample = mainFilter.process (sample);
            sample = dcBlock.process (sample);
            sample *= p.base.volume;

            float L, R;
            synthutil::panMonoToStereo (sample, p.base.pan, L, R);
            buffer.setSample (0, i, L);
            buffer.setSample (1, i, R);
        }

        synthutil::applyFadeOut (buffer, (int)(0.01f * sr));
        return buffer;
    }
};

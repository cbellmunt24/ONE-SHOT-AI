#pragma once

#include <JuceHeader.h>
#include "../Params/InstrumentParams.h"
#include "../DSP/Oscillator.h"
#include "../DSP/NoiseGenerator.h"
#include "../DSP/SynthFilter.h"
#include "../DSP/Envelope.h"
#include "../DSP/DCBlocker.h"
#include "SynthUtils.h"

// Texture synth — UPGRADED
//
// Mejoras:
//   — Pink/brown noise como base de granos (no solo white)
//   — FIX: tiltFilter separado por canal (ya no es mono)
//   — Noise coloreado por noiseColor

class TextureSynth
{
public:
    juce::AudioBuffer<float> generate (const TextureParams& p, double sampleRate, unsigned int seed)
    {
        float sr = (float) sampleRate;

        float duration = p.base.attack + p.base.decay + p.base.release + 1.5f;
        int numSamples = synthutil::durationInSamples (duration, sampleRate);

        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        dsputil::NoiseGenerator noise;
        noise.setSeed (seed);

        std::mt19937 grainRng (seed + 1);
        std::uniform_real_distribution<float> grainDist (-1.0f, 1.0f);

        // Filtro espectral (tilt) — UNO POR CANAL (fix del bug mono)
        dsputil::SVFilter tiltFilterL, tiltFilterR;
        FilterType tiltType = (p.spectralTilt >= 0.0f) ? FilterType::HighPass : FilterType::LowPass;
        float tiltCutoff = 300.0f + std::abs (p.spectralTilt) * 8000.0f;
        tiltFilterL.setParameters (tiltCutoff, 0.1f, tiltType, sr);
        tiltFilterR.setParameters (tiltCutoff, 0.1f, tiltType, sr);

        // Filtro principal — UNO POR CANAL
        dsputil::SVFilter mainFilterL, mainFilterR;
        mainFilterL.setParameters (p.base.filterCutoff, p.base.filterResonance,
                                   p.base.filterType, sr);
        mainFilterR.setParameters (p.base.filterCutoff, p.base.filterResonance,
                                   p.base.filterType, sr);

        // Envelope global con ataque curvado
        dsputil::ADSREnvelope ampEnv;
        ampEnv.setParameters (p.base.attack, p.base.decay, 0.6f, p.base.release + 0.5f, sr);
        ampEnv.setAttackCurve (0.8f);
        ampEnv.trigger();

        int releaseAt = synthutil::durationInSamples (p.base.attack + p.base.decay + 1.0f, sampleRate);

        // --- Sistema granular ---
        float grainInterval = 1.0f / std::max (1.0f, p.density * 200.0f);
        int grainIntervalSamples = std::max (1, (int) (grainInterval * sr));
        int grainSizeSamples = std::max (10, (int) (p.grainSize * sr));

        struct Grain
        {
            int startSample;
            int lengthSamples;
            float pan;
            float pitch;
            float noiseColor;  // color de noise por grano
        };

        std::vector<Grain> grains;
        int nextGrainSample = 0;

        while (nextGrainSample < numSamples)
        {
            Grain g;
            float scatterOffset = grainDist (grainRng) * p.scatter * grainIntervalSamples;
            g.startSample = nextGrainSample + (int) scatterOffset;
            g.startSample = std::max (0, std::min (g.startSample, numSamples - 1));

            float sizeVar = 1.0f + grainDist (grainRng) * 0.3f;
            g.lengthSamples = std::max (10, (int) ((float) grainSizeSamples * sizeVar));

            g.pan = 0.5f + grainDist (grainRng) * p.stereoWidth * 0.5f;
            g.pan = synthutil::clamp (g.pan, 0.0f, 1.0f);

            g.pitch = 1.0f + grainDist (grainRng) * p.movement * 0.2f;
            g.noiseColor = p.noiseColor + grainDist (grainRng) * 0.15f;
            g.noiseColor = synthutil::clamp (g.noiseColor, 0.0f, 1.0f);

            grains.push_back (g);
            nextGrainSample += grainIntervalSamples;
        }

        // --- Renderizar granos con noise coloreado ---
        for (const auto& g : grains)
        {
            for (int j = 0; j < g.lengthSamples; ++j)
            {
                int idx = g.startSample + j;
                if (idx < 0 || idx >= numSamples) continue;

                float windowPhase = (float) j / (float) g.lengthSamples;
                float window = 0.5f * (1.0f - std::cos (dsputil::TWO_PI * windowPhase));

                // Noise coloreado: brown/pink/white segun noiseColor del grano
                float grain = noise.nextColored (g.noiseColor) * window;

                float L, R;
                synthutil::panMonoToStereo (grain, g.pan, L, R);

                float grainAmp = 1.0f / std::max (1.0f, std::sqrt (p.density * 40.0f));
                buffer.setSample (0, idx, buffer.getSample (0, idx) + L * grainAmp);
                buffer.setSample (1, idx, buffer.getSample (1, idx) + R * grainAmp);
            }
        }

        // --- Procesamiento post-granular ---
        dsputil::DCBlocker dcL, dcR;

        for (int i = 0; i < numSamples; ++i)
        {
            if (i == releaseAt)
                ampEnv.startRelease();

            float env = ampEnv.next();
            if (! ampEnv.isActive() && i > releaseAt + (int) (0.1f * sr))
                break;

            // Canal L
            float sampleL = buffer.getSample (0, i);
            if (std::abs (p.spectralTilt) > 0.05f)
                sampleL = tiltFilterL.process (sampleL);
            sampleL = mainFilterL.process (sampleL);
            sampleL = dcL.process (sampleL);
            sampleL *= env * p.base.volume;
            buffer.setSample (0, i, sampleL);

            // Canal R
            float sampleR = buffer.getSample (1, i);
            if (std::abs (p.spectralTilt) > 0.05f)
                sampleR = tiltFilterR.process (sampleR);
            sampleR = mainFilterR.process (sampleR);
            sampleR = dcR.process (sampleR);
            sampleR *= env * p.base.volume;
            buffer.setSample (1, i, sampleR);
        }

        // --- Movement ---
        if (p.movement > 0.01f)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                float t = (float) i / sr;
                float mod = 1.0f - p.movement * 0.3f * (1.0f - std::cos (dsputil::TWO_PI * p.evolutionRate * 2.0f * t));
                buffer.setSample (0, i, buffer.getSample (0, i) * mod);
                buffer.setSample (1, i, buffer.getSample (1, i) * mod);
            }
        }

        synthutil::applyFadeOut (buffer, (int) (0.05f * sr));
        return buffer;
    }
};

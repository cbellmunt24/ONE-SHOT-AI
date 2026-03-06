#pragma once

#include <JuceHeader.h>
#include "../Params/InstrumentParams.h"
#include "../DSP/Oscillator.h"
#include "../DSP/Envelope.h"
#include "../DSP/NoiseGenerator.h"
#include "../DSP/SynthFilter.h"
#include "../DSP/Saturator.h"
#include "SynthUtils.h"

// Snare profesional nivel Splice — UPGRADED con pink noise
//
// Mejoras:
//   — Pink noise como base → percusion mucho mas natural, menos hissy
//   — Cascada de filtros (BP tight + LP) para "crack" no "hiss"
//   — Tape saturation para cohesion calida

class SnareSynth
{
public:
    juce::AudioBuffer<float> generate (const SnareParams& p, double sampleRate, unsigned int seed)
    {
        const float sr = (float) sampleRate;

        float duration = std::max (p.bodyDecay, p.noiseDecay) * 2.5f + 0.08f;
        int numSamples = synthutil::durationInSamples (duration, sampleRate);

        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        dsputil::NoiseGenerator noise;
        noise.setSeed (seed);

        double bodyPhase = 0.0;

        // === NOISE SHAPING: cascada BP → LP para eliminar hiss ===
        dsputil::SVFilter noiseBP1, noiseBP2;
        float bpFreq = 1200.0f + p.noiseColor * 4000.0f;
        float bpQ = 0.15f + p.noiseTightness * 0.55f;
        noiseBP1.setParameters (bpFreq, bpQ, FilterType::BandPass, sr);
        noiseBP2.setParameters (bpFreq, bpQ * 0.8f, FilterType::BandPass, sr);

        // LP post-BP
        dsputil::SVFilter noiseLP;
        float lpFreq = std::min (p.noiseLP, sr * 0.48f);
        noiseLP.setParameters (lpFreq, 0.05f, FilterType::LowPass, sr);

        // === WIRE ===
        dsputil::SVFilter wireFilter;
        wireFilter.setParameters (5500.0f + p.noiseColor * 2500.0f, 0.45f,
                                  FilterType::BandPass, sr);
        dsputil::SVFilter wireLP;
        wireLP.setParameters (9000.0f, 0.1f, FilterType::LowPass, sr);

        // === SNAP ===
        dsputil::SVFilter snapBP;
        snapBP.setParameters (2500.0f + p.noiseColor * 2000.0f, 0.30f,
                              FilterType::BandPass, sr);

        // Main filter
        dsputil::SVFilter mainFilter;
        mainFilter.setParameters (p.base.filterCutoff, p.base.filterResonance,
                                  p.base.filterType, sr);

        dsputil::Saturator saturator;

        for (int i = 0; i < numSamples; ++i)
        {
            float t = (float) i / sr;

            // === BODY ===
            float pitchDrop = std::exp (-t / 0.004f);
            float bodyFreq = p.bodyFreq * (1.0f + pitchDrop * 0.6f);

            bodyPhase += (double) bodyFreq / (double) sr;
            if (bodyPhase >= 1.0) bodyPhase -= 1.0;

            float bodyAngle = (float)(bodyPhase * dsputil::TWO_PI);
            float body = std::sin (bodyAngle) * 0.7f
                       + std::sin (bodyAngle * 2.0f) * 0.3f;

            float bodyEnv = std::exp (-t / std::max (0.003f, p.bodyDecay * 0.6f));

            // === NOISE: PINK como base (mucho mas natural que white) ===
            float noiseRaw = noise.nextPink();
            float noiseWhite = noise.next();

            // Cascada BP
            float noiseShaped = noiseBP1.process (noiseRaw);
            noiseShaped = noiseBP2.process (noiseShaped);
            noiseShaped = noiseLP.process (noiseShaped);
            float noiseEnv = std::exp (-t / std::max (0.001f, p.noiseDecay));

            // === SNAP ===
            float snapEnv = std::exp (-t / 0.0004f);
            float snap = snapBP.process (noiseWhite) * snapEnv * p.snapAmount * 1.2f;

            // === WIRE ===
            float wire = wireFilter.process (noiseRaw);
            wire = wireLP.process (wire);
            float wireEnv = std::exp (-t / std::max (0.001f, p.noiseDecay * 1.0f));

            // === MIX (body más presente, noise controlado) ===
            float bodyLevel = 0.2f + p.bodyMix * 0.8f;
            float noiseLevel = 0.5f + (1.0f - p.bodyMix) * 0.5f;

            float sample = body * bodyEnv * bodyLevel * 0.75f
                         + noiseShaped * noiseEnv * noiseLevel * 0.55f
                         + snap
                         + wire * wireEnv * p.wireAmount * 0.20f;

            // === TAPE SATURATION (mas calida que tanh cruda) ===
            if (p.base.saturationAmount > 0.01f)
            {
                sample = saturator.process (sample, p.base.saturationAmount,
                                            dsputil::SaturationMode::Tape);
            }

            sample = mainFilter.process (sample);
            sample *= p.base.volume;

            float L, R;
            synthutil::panMonoToStereo (sample, p.base.pan, L, R);
            buffer.setSample (0, i, L);
            buffer.setSample (1, i, R);
        }

        synthutil::applyFadeOut (buffer, (int)(0.008f * sr));
        return buffer;
    }
};

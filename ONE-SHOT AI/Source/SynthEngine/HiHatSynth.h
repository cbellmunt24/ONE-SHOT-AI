#pragma once

#include <JuceHeader.h>
#include "../Params/InstrumentParams.h"
#include "../DSP/Oscillator.h"
#include "../DSP/NoiseGenerator.h"
#include "../DSP/SynthFilter.h"
#include "SynthUtils.h"

// Hi-hat profesional v3
//
// Modelo basado en TR-808 real:
//   — 6 square-wave oscillators con ratios inarmónicos (no sines)
//   — Ring modulation entre pares para espectro inarmónico de platillo
//   — Noise shapeado con bandpass (6-12 kHz) como un platillo real
//   — Envolventes separadas: metal decae rápido, noise sustains más
//   — Transiente con bandpass agudo ("tick" de baqueta)

class HiHatSynth
{
public:
    juce::AudioBuffer<float> generate (const HiHatParams& p, double sampleRate, unsigned int seed)
    {
        const float sr = (float) sampleRate;

        float effectiveDecay = p.closedDecay
                             + p.openAmount * (p.openDecay - p.closedDecay);

        float duration = effectiveDecay * 4.5f + 0.06f;
        int numSamples = synthutil::durationInSamples (duration, sampleRate);

        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        // === TR-808 metallic ratios (6 oscilladores inarmónicos) ===
        static constexpr float metallicRatios[6] = {
            1.0f, 1.4471f, 1.6170f, 1.9265f, 2.5028f, 2.6637f
        };

        double metalPhases[6] = {};

        const float maxFreq = sr * 0.45f;
        float metalFreqs[6];
        for (int j = 0; j < 6; ++j)
            metalFreqs[j] = std::min (p.freqRange * metallicRatios[j], maxFreq);

        dsputil::NoiseGenerator noise;
        noise.setSeed (seed);

        // === FILTROS ===

        // HP principal — carácter cerrado vs abierto
        dsputil::SVFilter hpFilter;
        hpFilter.setParameters (p.highPassFreq, 0.18f, FilterType::HighPass, sr);

        // BP para noise — simula la resonancia espectral del platillo (6k-12k)
        dsputil::SVFilter noiseBP;
        float noiseCenterFreq = 6000.0f + p.noiseColor * 6000.0f;
        noiseBP.setParameters (std::min (noiseCenterFreq, sr * 0.45f),
                               0.2f + p.noiseColor * 0.15f,
                               FilterType::BandPass, sr);

        // LP en noise — controla brillo máximo
        dsputil::SVFilter noiseLP;
        float noiseLPFreq = 9000.0f + p.noiseColor * 7000.0f;
        noiseLP.setParameters (std::min (noiseLPFreq, sr * 0.48f), 0.08f,
                               FilterType::LowPass, sr);

        // Resonancia metálica — modos de vibración del platillo
        dsputil::SVFilter ringBP;
        ringBP.setParameters (std::min (p.freqRange * 1.2f, maxFreq),
                              0.3f + p.ringAmount * 0.4f,
                              FilterType::BandPass, sr);

        // BP agudo para transiente ("tick" de baqueta contra metal)
        dsputil::SVFilter transientBP;
        transientBP.setParameters (std::min (10000.0f, sr * 0.45f), 0.35f,
                                   FilterType::BandPass, sr);

        dsputil::SVFilter mainFilter;
        mainFilter.setParameters (p.base.filterCutoff, p.base.filterResonance,
                                  p.base.filterType, sr);

        // Generador de ruido separado para transiente (no afecta el state del principal)
        dsputil::NoiseGenerator transientNoise;
        transientNoise.setSeed (seed + 7919);

        for (int i = 0; i < numSamples; ++i)
        {
            float t = (float) i / sr;

            // === METALLIC LAYER: square waves (como 808 real) ===
            float metalSquare = 0.0f;
            for (int j = 0; j < 6; ++j)
            {
                metalPhases[j] += (double) metalFreqs[j] / (double) sr;
                if (metalPhases[j] >= 1.0) metalPhases[j] -= 1.0;
                // Square wave — mucho más rico en armónicos que sine
                float sq = metalPhases[j] < 0.5 ? 1.0f : -1.0f;
                metalSquare += sq;
            }
            metalSquare /= 6.0f;

            // Ring mod entre pares de osciladores — genera frecuencias suma/diferencia
            // Esto crea el espectro denso e inarmónico característico de los platillos
            float ringMod = 0.0f;
            for (int j = 0; j < 6; j += 2)
            {
                float s1 = std::sin ((float)(metalPhases[j] * dsputil::TWO_PI));
                float s2 = std::sin ((float)(metalPhases[j + 1] * dsputil::TWO_PI));
                ringMod += s1 * s2;
            }
            ringMod /= 3.0f;

            // Blend: squares + ring mod (más metálico con más p.metallic)
            float metalMix = metalSquare * (1.0f - p.metallic * 0.4f)
                           + ringMod * p.metallic * 0.6f;

            // === NOISE LAYER: shaped como platillo ===
            float noiseRaw = noise.nextColored (p.noiseColor);
            // Doble shaping: BP para resonancia + LP para control de brillo
            float noiseBPed = noiseBP.process (noiseRaw);
            float noiseLPed = noiseLP.process (noiseRaw);
            float noiseShaped = noiseBPed * 0.6f + noiseLPed * 0.4f;

            // === ENVELOPES separadas por componente ===
            // Metal: decae rápido (modos metálicos se apagan antes)
            float metalDecay = effectiveDecay * 0.6f;
            float metalEnv = std::exp (-t / std::max (0.0008f, metalDecay));

            // Noise: decae más lento (el "sshh" del platillo persiste)
            float noiseDecay = effectiveDecay * 1.3f;
            float noiseEnv = std::exp (-t / std::max (0.001f, noiseDecay));

            // === TRANSIENT: "tick" agudo — baqueta golpeando metal ===
            float transientRaw = transientNoise.next();
            float transientShaped = transientBP.process (transientRaw);
            float transientEnv = std::exp (-t / 0.00035f);
            float transient = transientShaped * transientEnv * 0.55f;

            // === MIX final ===
            float sample = metalMix * metalEnv * p.metallic * 0.45f
                         + noiseShaped * noiseEnv * (1.0f - p.metallic * 0.25f) * 0.5f
                         + transient;

            // === HP FILTER ===
            sample = hpFilter.process (sample);

            // === RING RESONANCE (modos resonantes del platillo) ===
            if (p.ringAmount > 0.01f)
            {
                float ringSig = ringBP.process (sample);
                float ringEnv = std::exp (-t / std::max (0.001f, effectiveDecay * 0.45f));
                sample += ringSig * p.ringAmount * 0.2f * ringEnv;
            }

            sample = mainFilter.process (sample);
            sample *= p.base.volume;

            float L, R;
            synthutil::panMonoToStereo (sample, p.base.pan, L, R);
            buffer.setSample (0, i, L);
            buffer.setSample (1, i, R);
        }

        synthutil::applyFadeOut (buffer, (int)(0.005f * sr));
        return buffer;
    }
};

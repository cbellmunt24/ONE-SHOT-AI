#pragma once

#include <JuceHeader.h>
#include "../Params/InstrumentParams.h"
#include "../DSP/Oscillator.h"
#include "../DSP/NoiseGenerator.h"
#include "../DSP/SynthFilter.h"
#include "../DSP/DelayLine.h"
#include "../DSP/DCBlocker.h"
#include "SynthUtils.h"

// Pluck synth — UPGRADED con Hermite interpolation
//
// Mejoras:
//   — readHermite() en Karplus-Strong delay → afinacion mas precisa, menos perdida HF
//   — Two-point average damping (mas natural que one-pole)
//   — Pink noise para excitacion mas rica

class PluckSynth
{
public:
    juce::AudioBuffer<float> generate (const PluckParams& p, double sampleRate, unsigned int seed)
    {
        float sr = (float) sampleRate;

        float duration = p.decayTime + p.base.release + 0.1f;
        int numSamples = synthutil::durationInSamples (duration, sampleRate);

        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        float baseFreq = synthutil::midiToFreq (p.base.pitchBase);

        // --- Karplus-Strong ---
        float delayLength = sr / baseFreq;
        int delaySize = (int) delayLength + 4;  // +4 para Hermite

        dsputil::DelayLine delay;
        delay.setSize (delaySize);

        dsputil::NoiseGenerator noise;
        noise.setSeed (seed);

        // Damping: one-pole LP en feedback
        dsputil::OnePole dampingFilter;
        float dampCutoff = baseFreq * (2.0f + (1.0f - p.damping) * 20.0f);
        dampingFilter.setLowPass (dampCutoff, sr);

        // Brillo de excitacion
        dsputil::SVFilter brightnessFilter;
        float brightCutoff = 500.0f + p.brightness * 18000.0f;
        brightnessFilter.setParameters (brightCutoff, 0.1f, FilterType::LowPass, sr);

        dsputil::SVFilter mainFilter;
        mainFilter.setParameters (p.base.filterCutoff, p.base.filterResonance,
                                  p.base.filterType, sr);

        int excitationSamples = std::max (2, (int) (delayLength));

        // Body resonance
        dsputil::DelayLine bodyDelay;
        int bodyDelaySize = (int) (sr / (baseFreq * 1.5f)) + 4;
        bodyDelay.setSize (bodyDelaySize);

        int pickDelay = std::max (1, (int) (delayLength * p.pickPosition));

        // Previo para two-point average
        float prevDelayed = 0.0f;

        dsputil::DCBlocker dcL, dcR;

        for (int i = 0; i < numSamples; ++i)
        {
            float t = (float) i / sr;

            // --- Excitacion: pink noise (mas rica que white) ---
            float excitation = 0.0f;
            if (i < excitationSamples)
            {
                excitation = noise.nextPink();
                excitation = brightnessFilter.process (excitation);

                if (i >= pickDelay)
                    excitation -= delay.read (pickDelay) * 0.3f;
            }

            // --- Karplus-Strong con Hermite interpolation ---
            float delayed = delay.readHermite (delayLength);

            // Two-point average: (current + previous) * 0.5 → damping mas natural
            float averaged = (delayed + prevDelayed) * 0.5f;
            prevDelayed = delayed;

            // Damping filter
            float filtered = dampingFilter.process (averaged);

            // Feedback
            float feedback = 0.996f - (1.0f - p.stringTension) * 0.01f;
            float toWrite = excitation + filtered * feedback;

            delay.write (toWrite);

            float sample = delayed;

            // --- Body resonance ---
            if (p.bodyResonance > 0.01f)
            {
                bodyDelay.write (sample);
                float bodySignal = bodyDelay.readHermite ((float) bodyDelaySize - 1.0f);
                sample += bodySignal * p.bodyResonance * 0.45f;
            }

            // --- Harmonics ---
            if (p.harmonics > 0.01f)
                sample = sample * (1.0f - p.harmonics * 0.3f)
                       + std::tanh (sample * 1.5f) * p.harmonics * 0.3f;

            // --- Main filter ---
            sample = mainFilter.process (sample);
            sample *= p.base.volume;

            // --- Stereo width (más pronunciado) ---
            float L = sample;
            float R = sample;
            if (p.stereoWidth > 0.01f)
            {
                float phase = std::sin (dsputil::TWO_PI * 0.7f * t + 0.3f);
                float widthAmt = p.stereoWidth * 0.25f;
                L *= (1.0f + phase * widthAmt);
                R *= (1.0f - phase * widthAmt);
            }

            // DC block
            L = dcL.process (L);
            R = dcR.process (R);

            float panL = std::cos (p.base.pan * dsputil::PI * 0.5f);
            float panR = std::sin (p.base.pan * dsputil::PI * 0.5f);
            buffer.setSample (0, i, L * panL);
            buffer.setSample (1, i, R * panR);
        }

        synthutil::applyFadeOut (buffer, (int) (0.02f * sr));
        return buffer;
    }
};

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

        // --- FM synthesis oscillators ---
        dsputil::Oscillator fmCarrier, fmModulator, fmCarrier2, fmModulator2;
        fmCarrier.reset (0.0f);
        fmModulator.reset (0.0f);
        fmCarrier2.reset (0.25f);   // offset phase for detuned pair
        fmModulator2.reset (0.25f);

        fmCarrier.setFrequency (baseFreq, sr);
        fmModulator.setFrequency (baseFreq * 2.0f, sr);  // mod ratio 2:1
        fmCarrier2.setFrequency (baseFreq * 1.005f, sr);  // slight detune
        fmModulator2.setFrequency (baseFreq * 3.0f * 1.01f, sr);  // mod ratio 3:1, detuned

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

            float ksSample = delayed;

            // --- FM synthesis layer ---
            float fmSample = 0.0f;
            if (p.fmAmount > 0.01f)
            {
                // FM index decays over time (bright attack -> mellow tail)
                float fmIndex = p.fmAmount * 5.0f * std::exp (-t / std::max (p.decayTime * 0.7f, 0.01f));

                // Modulator 1
                float mod1 = fmModulator.next (OscillatorType::Sine);
                // Carrier 1 with FM: phase deviation via frequency modulation
                float fmDeviation = fmIndex * mod1 * baseFreq * 2.0f;
                fmCarrier.setFrequency (baseFreq + fmDeviation, sr);
                float fm1 = fmCarrier.next (OscillatorType::Sine);

                // Second FM pair (detuned, softer) for richness
                float mod2 = fmModulator2.next (OscillatorType::Sine);
                float fmDeviation2 = fmIndex * 0.6f * mod2 * baseFreq * 3.0f;
                fmCarrier2.setFrequency (baseFreq * 1.005f + fmDeviation2, sr);
                float fm2 = fmCarrier2.next (OscillatorType::Sine);

                float ampEnv = std::exp (-t / std::max (p.decayTime, 0.001f));
                fmSample = (fm1 + fm2 * 0.4f) * ampEnv;
            }

            // Blend KS and FM
            float sample = ksSample * (1.0f - p.fmAmount) + fmSample * p.fmAmount;

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

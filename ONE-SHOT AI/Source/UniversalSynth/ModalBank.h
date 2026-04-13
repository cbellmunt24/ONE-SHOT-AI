#pragma once

#include <cmath>
#include <array>
#include <algorithm>
#include "../DSP/DSPConstants.h"
#include "../DSP/SynthFilter.h"
#include "../DSP/DelayLine.h"
#include "../DSP/NoiseGenerator.h"

namespace universalsynth
{

// ==========================================================================
// ModalBank — Bank of tunable resonant modes + Karplus-Strong string model
//
// Two modes of operation controlled by modalMode param:
//   modalMode < 0.5: Modal resonators (bank of bandpass filters)
//                     Good for: bells, metallic percussion, cymbals, body resonances
//   modalMode >= 0.5: Karplus-Strong (delay line with filtered feedback)
//                     Good for: plucks, strings, tuned percussion
//
// The modal bank can produce inharmonic spectra (like hi-hats and cymbals)
// by spreading mode frequencies away from harmonic ratios.
// ==========================================================================

class ModalBank
{
public:
    static constexpr int MAX_MODES = 12;

    void prepare (float sampleRate)
    {
        sr = sampleRate;

        for (int i = 0; i < MAX_MODES; ++i)
        {
            modeFilters[i].reset();
            modeEnvLevels[i] = 0.0f;
        }

        // KS delay line: enough for 15 Hz fundamental + Hermite interpolation margin
        ksDelay.setSize ((int) (sampleRate / 15.0f) + 16);
        ksDelay.clear();
        ksLoopFilter.reset();
        ksBodyFilter.reset();
        ksExcitationDone = false;

        excitationNoise.setSeed (42);
    }

    // Reset state for a new note
    void reset()
    {
        for (int i = 0; i < MAX_MODES; ++i)
        {
            modeFilters[i].reset();
            modeEnvLevels[i] = 1.0f;
        }
        ksDelay.clear();
        ksLoopFilter.reset();
        ksBodyFilter.reset();
        ksExcitationDone = false;
        ksSampleCount = 0;
    }

    // Configure the modal bank for a specific sound
    void configure (float fundamentalHz, int numActiveModes, float spread,
                    float ratioBase, float damping, float decayTime,
                    float sampleRate)
    {
        sr = sampleRate;
        activeModes = std::min (numActiveModes, MAX_MODES);

        // Compute mode frequencies based on spread parameter
        // spread=0: harmonic series (1, 2, 3, 4...)
        // spread=0.5: slightly inharmonic (realistic metallic)
        // spread=1.0: highly inharmonic (TR-808 hihat ratios)
        computeModeFrequencies (fundamentalHz, ratioBase, spread);

        // Configure each mode's bandpass filter and decay
        for (int i = 0; i < activeModes; ++i)
        {
            float freq = modeFreqs[i];
            // Clamp below Nyquist — 0.45 is safe for Cytomic SVF (tan(pi*f/sr) stays finite)
            freq = std::max (30.0f, std::min (freq, sampleRate * 0.45f));

            // Q increases with mode number for more ring
            // Reduce Q when approaching Nyquist to prevent SVFilter ringing/instability
            float nyquistProximity = freq / (sampleRate * 0.45f); // 0..1
            float qDampen = (nyquistProximity > 0.7f) ? 1.0f - (nyquistProximity - 0.7f) * 2.5f : 1.0f;
            qDampen = std::max (0.15f, qDampen);
            float baseQ = 0.3f + (1.0f - damping) * 0.65f;
            float q = (baseQ + (float) i * 0.02f) * qDampen;
            q = std::min (q, 0.9f);

            modeFilters[i].setParameters (freq, q, FilterType::BandPass, sampleRate);

            // Each mode decays independently: higher modes decay faster (damping)
            float modeDecayTime = decayTime * (1.0f - damping * (float) i / (float) activeModes * 0.7f);
            modeDecayTime = std::max (modeDecayTime, 0.005f);
            modeDecayCoeffs[i] = std::exp (-6.9f / (modeDecayTime * sampleRate));

            // Mode amplitudes: higher modes are quieter
            float ampFalloff = 1.0f / (1.0f + (float) i * 0.3f);
            modeAmplitudes[i] = ampFalloff;

            modeEnvLevels[i] = 1.0f;
        }
    }

    // Configure Karplus-Strong parameters
    void configureKS (float fundamentalHz, float feedback, float damping,
                      float brightness, float pickPosition, float bodyResonance,
                      float sampleRate)
    {
        sr = sampleRate;

        // Delay length for target pitch (clamped to safe range for delay buffer)
        float maxDelay = sampleRate / 15.0f;
        float delayLength = sampleRate / std::max (fundamentalHz, 20.0f);
        ksDelayLength = std::min (delayLength, maxDelay);

        ksFeedbackGain = feedback;

        // Loop filter: one-pole LP that controls string damping
        // Higher damping = lower cutoff = duller, shorter sustain
        float lpCutoff = 200.0f + (1.0f - damping) * (sampleRate * 0.4f - 200.0f);
        ksLoopFilter.setLowPass (lpCutoff, sampleRate);

        // Excitation: noise filtered by brightness
        ksBrightness = brightness;

        // Pick position creates a comb filter notch
        ksPickPos = std::max (0.01f, std::min (pickPosition, 0.99f));

        // Body resonance: coupled resonator
        if (bodyResonance > 0.01f)
        {
            float bodyFreq = fundamentalHz * 0.8f; // body resonance slightly below fundamental
            float bodyQ = 0.2f + bodyResonance * 0.5f;
            ksBodyFilter.setParameters (bodyFreq, bodyQ, FilterType::BandPass, sampleRate);
            ksBodyAmount = bodyResonance;
        }
        else
        {
            ksBodyAmount = 0.0f;
        }

        ksExcitationLength = (int) delayLength;
        ksExcitationDone = false;
        ksSampleCount = 0;
    }

    // Process one sample in modal resonator mode
    // excitation: input signal to excite the modes (noise burst, impulse, etc.)
    float processModal (float excitation)
    {
        float output = 0.0f;

        for (int i = 0; i < activeModes; ++i)
        {
            float modeOut = modeFilters[i].process (excitation);

            // Apply per-mode envelope decay
            modeOut *= modeEnvLevels[i] * modeAmplitudes[i];
            modeEnvLevels[i] *= modeDecayCoeffs[i];

            output += modeOut;
        }

        // Normalize by number of active modes to prevent clipping
        if (activeModes > 1)
            output /= std::sqrt ((float) activeModes) * 0.7f;

        // NaN/Inf safety
        if (std::isnan (output) || std::isinf (output))
            output = 0.0f;
        return std::max (-1.5f, std::min (output, 1.5f));
    }

    // Process one sample in Karplus-Strong mode
    float processKS()
    {
        float excitation = 0.0f;

        // Excitation: fill the delay line once with shaped noise
        if (! ksExcitationDone)
        {
            // Colored noise excitation
            float noise = (ksBrightness > 0.5f)
                ? excitationNoise.next()
                : excitationNoise.nextColored (ksBrightness);

            // Pick position comb filter: notch at sampleRate / (pickPos * delayLength)
            // Implemented as subtraction of delayed copy
            excitation = noise;

            ++ksSampleCount;
            if (ksSampleCount >= ksExcitationLength)
                ksExcitationDone = true;
        }

        // Read from delay line with Hermite interpolation
        float delayed = ksDelay.readHermite (ksDelayLength);

        // Loop filter (one-pole LP for damping)
        float filtered = ksLoopFilter.process (delayed);

        // Feedback into delay line
        float feedback = excitation + filtered * ksFeedbackGain;

        // Pick position comb: read at pick position for notch effect
        if (ksPickPos > 0.01f && ksPickPos < 0.99f)
        {
            float pickDelay = ksDelayLength * ksPickPos;
            float pickSample = ksDelay.readHermite (pickDelay);
            // Subtract delayed copy: creates notch at harmonics of 1/(pickPos*period)
            feedback -= pickSample * 0.3f;
        }

        ksDelay.write (feedback);

        float output = delayed;

        // Body resonance coupling
        if (ksBodyAmount > 0.01f)
        {
            float bodyOut = ksBodyFilter.process (output);
            output += bodyOut * ksBodyAmount * 0.5f;
        }

        // NaN/Inf safety
        if (std::isnan (output) || std::isinf (output))
            output = 0.0f;
        return std::max (-1.5f, std::min (output, 1.5f));
    }

    int getActiveModes() const { return activeModes; }

private:
    float sr = 44100.0f;
    int activeModes = 6;

    // Modal resonator state
    dsputil::SVFilter modeFilters[MAX_MODES];
    float modeFreqs[MAX_MODES] = {};
    float modeDecayCoeffs[MAX_MODES] = {};
    float modeAmplitudes[MAX_MODES] = {};
    float modeEnvLevels[MAX_MODES] = {};

    // Karplus-Strong state
    dsputil::DelayLine ksDelay;
    dsputil::OnePole ksLoopFilter;
    dsputil::SVFilter ksBodyFilter;
    float ksDelayLength = 100.0f;
    float ksFeedbackGain = 0.995f;
    float ksBrightness = 0.5f;
    float ksPickPos = 0.5f;
    float ksBodyAmount = 0.0f;
    int ksExcitationLength = 100;
    bool ksExcitationDone = false;
    int ksSampleCount = 0;
    dsputil::NoiseGenerator excitationNoise;

    // Compute mode frequencies based on inharmonicity spread
    void computeModeFrequencies (float fundamental, float ratioBase, float spread)
    {
        // TR-808 hi-hat inharmonic ratios (classic metallic sound)
        static constexpr float hihatRatios[12] = {
            1.0f, 1.447f, 1.617f, 1.927f, 2.503f, 2.664f,
            3.113f, 3.527f, 4.012f, 4.553f, 5.101f, 5.687f
        };

        // Harmonic series ratios
        static constexpr float harmonicRatios[12] = {
            1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f,
            7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f
        };

        float baseFreq = fundamental * ratioBase;

        for (int i = 0; i < MAX_MODES; ++i)
        {
            // Interpolate between harmonic and inharmonic ratios
            float ratio = harmonicRatios[i] * (1.0f - spread) + hihatRatios[i] * spread;
            modeFreqs[i] = baseFreq * ratio;

            // Clamp to safe range
            modeFreqs[i] = std::max (20.0f, std::min (modeFreqs[i], sr * 0.45f));
        }
    }
};

} // namespace universalsynth

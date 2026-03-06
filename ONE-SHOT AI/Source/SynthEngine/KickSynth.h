#pragma once

#include <JuceHeader.h>
#include "../Params/InstrumentParams.h"
#include "../DSP/Oscillator.h"
#include "../DSP/NoiseGenerator.h"
#include "../DSP/SynthFilter.h"
#include "../DSP/Saturator.h"
#include "../DSP/DCBlocker.h"
#include "SynthUtils.h"

// Kick drum profesional — UPGRADED
//
// Mejoras:
//   — Tape saturation (mas calida que tanh cruda)
//   — DC blocker reutilizable
//   — Click con pink noise para transiente mas natural

class KickSynth
{
public:
    juce::AudioBuffer<float> generate (const KickParams& p, double sampleRate, unsigned int seed)
    {
        const float sr = (float) sampleRate;

        float duration = p.base.attack + p.bodyDecay + p.tailLength + 0.1f;
        int numSamples = synthutil::durationInSamples (duration, sampleRate);

        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        dsputil::NoiseGenerator noise;
        noise.setSeed (seed);

        double bodyPhase = 0.0;
        double subPhase = 0.0;

        dsputil::SVFilter clickBP;
        float clickCenter = 3500.0f + p.clickAmount * 4500.0f;
        clickBP.setParameters (clickCenter, 0.3f + p.clickAmount * 0.15f,
                               FilterType::BandPass, sr);

        dsputil::SVFilter subLP;
        subLP.setParameters (std::min (p.subFreq * 4.0f, sr * 0.48f), 0.0f,
                             FilterType::LowPass, sr);

        dsputil::SVFilter mainFilter;
        mainFilter.setParameters (p.base.filterCutoff, p.base.filterResonance,
                                  p.base.filterType, sr);

        const float attackSec = std::max (0.0001f, p.base.attack);
        const float targetFreq = p.subFreq;
        const float startFreq = targetFreq * std::pow (2.0f, p.pitchDrop / 12.0f);

        dsputil::DCBlocker dcBlock;
        dsputil::Saturator saturator;

        for (int i = 0; i < numSamples; ++i)
        {
            float t = (float) i / sr;

            // === MULTI-STAGE PITCH ENVELOPE ===
            float fastT = std::max (0.0005f, p.pitchDropTime * 0.2f);
            float slowT = std::max (0.002f, p.pitchDropTime * 1.5f);
            float pitchEnv = std::exp (-t / fastT) * 0.65f
                           + std::exp (-t / slowT) * 0.35f;
            float bodyFreq = targetFreq + (startFreq - targetFreq) * pitchEnv;

            // === PHASE ACCUMULATION ===
            bodyPhase += (double) bodyFreq / (double) sr;
            if (bodyPhase >= 1.0) bodyPhase -= 1.0;

            subPhase += (double) targetFreq / (double) sr;
            if (subPhase >= 1.0) subPhase -= 1.0;

            // === BODY ===
            float body = std::sin ((float)(bodyPhase * dsputil::TWO_PI));

            float bodyEnv;
            if (t < attackSec)
                bodyEnv = t / attackSec;
            else
            {
                float dt = t - attackSec;
                float punch = std::exp (-dt / std::max (0.001f, p.bodyDecay * 0.4f));
                float sustain = std::exp (-dt / std::max (0.001f, p.bodyDecay * 1.6f));
                bodyEnv = punch * 0.55f + sustain * 0.45f;
            }

            // === SUB (controlado — sin exceso de bajo) ===
            float sub = std::sin ((float)(subPhase * dsputil::TWO_PI));
            sub = subLP.process (sub);

            float subAttack = attackSec * 1.2f;
            float subEnv;
            if (t < subAttack)
                subEnv = t / subAttack;
            else
            {
                // Decay más rápido: punch prominente, sustain controlado
                float dt = t - subAttack;
                float subPunch = std::exp (-dt / std::max (0.008f, p.tailLength * 0.25f));
                float subSustain = std::exp (-dt / std::max (0.01f, p.tailLength * 1.0f));
                subEnv = subPunch * 0.55f + subSustain * 0.45f;
            }

            // === CLICK: pink noise para transiente mas natural ===
            float noiseRaw = noise.nextPink();
            float clickShaped = clickBP.process (noiseRaw);
            float clickDecayTime = 0.0005f + (1.0f - p.clickAmount) * 0.003f;
            float clickEnv = std::exp (-t / clickDecayTime);
            float click = clickShaped * clickEnv * 0.8f * p.clickAmount;

            float topEnv = std::exp (-t / 0.0003f);
            float top = noise.next() * topEnv * 0.15f * p.clickAmount;

            // === MIX (body dominante, sub controlado) ===
            float sample = body * bodyEnv * 0.6f
                         + sub * subEnv * p.subLevel * 0.35f
                         + click * 0.45f + top;

            // === TAPE SATURATION (calida, preserva transientes) ===
            if (p.driveAmount > 0.01f)
            {
                sample = saturator.process (sample, p.driveAmount,
                                            dsputil::SaturationMode::Tape);
            }

            // === FILTER ===
            sample = mainFilter.process (sample);

            // === DC BLOCK ===
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

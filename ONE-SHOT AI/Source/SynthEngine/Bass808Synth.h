#pragma once

#include <JuceHeader.h>
#include "../Params/InstrumentParams.h"
#include "../DSP/Oscillator.h"
#include "../DSP/Envelope.h"
#include "../DSP/SynthFilter.h"
#include "../DSP/Saturator.h"
#include "../DSP/DCBlocker.h"
#include "SynthUtils.h"

// Bass 808 premium v3
//
// Dos modos según reeseMix:
//   — reeseMix ≈ 0: Classic 808 sine bass (Trap, HipHop, etc.)
//   — reeseMix > 0: Reese bass (Reggaeton, DnB, etc.)
//
// Reese bass features:
//   — 3 saws detuned con PolyBLEP (no solo 2)
//   — Detune LFO: modula el spread → "wobble" característico
//   — Filter LFO: LP que se abre/cierra → "growl"
//   — Stereo spread real: saws distribuidos en el panorama
//   — Tube saturation para riqueza armónica

class Bass808Synth
{
public:
    juce::AudioBuffer<float> generate (const Bass808Params& p, double sampleRate, unsigned int seed)
    {
        const float sr = (float) sampleRate;

        const float attackSec = std::max (0.001f, p.base.attack);
        const float decaySec  = std::max (0.01f, p.base.decay);
        const float tailSec   = std::max (0.1f, p.tailLength);

        float duration = attackSec + tailSec * 2.5f;
        int numSamples = synthutil::durationInSamples (duration, sampleRate);

        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        double mainPhase = 0.0;
        double subPhase  = 0.0;

        // === REESE: 3 saws detuned ===
        dsputil::Oscillator reeseSaw1, reeseSaw2, reeseSaw3;
        reeseSaw1.reset (0.0f);
        reeseSaw2.reset (0.33f);
        reeseSaw3.reset (0.66f);

        dsputil::SVFilter bodyFilter;
        bodyFilter.setParameters (p.base.filterCutoff, p.base.filterResonance,
                                  p.base.filterType, sr);

        // Filtro con envolvente (donk bass)
        dsputil::SVFilter envFilter;
        float envFilterBaseFreq = p.base.filterCutoff;

        // === REESE FILTER: LP con LFO para el "growl" ===
        dsputil::SVFilter reeseFilterL, reeseFilterR;

        const float targetFreq = p.subFreq;
        const float startFreq  = targetFreq * std::pow (2.0f, p.glideAmount / 12.0f);

        dsputil::DCBlocker dcBlockL, dcBlockR;
        dsputil::Saturator saturator;

        dsputil::SVFilter punchBP;
        punchBP.setParameters (targetFreq * 3.0f, 0.3f, FilterType::BandPass, sr);

        // Seed para LFOs deterministas
        std::mt19937 lfoRng (seed);
        std::uniform_real_distribution<float> lfoDist (0.0f, dsputil::TWO_PI);
        float lfoPhaseOffset = lfoDist (lfoRng);

        for (int i = 0; i < numSamples; ++i)
        {
            float t = (float) i / sr;

            // === PITCH GLIDE ===
            float currentFreq;
            if (p.glideTime > 0.001f)
            {
                float glideEnv = std::exp (-t / p.glideTime);
                currentFreq = targetFreq + (startFreq - targetFreq) * glideEnv;
            }
            else
            {
                currentFreq = targetFreq;
            }

            // === 808 SINE ===
            mainPhase += (double) currentFreq / (double) sr;
            if (mainPhase >= 1.0) mainPhase -= 1.0;

            float sinBody = std::sin ((float)(mainPhase * dsputil::TWO_PI));

            subPhase += (double) currentFreq * 0.5 / (double) sr;
            if (subPhase >= 1.0) subPhase -= 1.0;

            float subOct = std::sin ((float)(subPhase * dsputil::TWO_PI));
            float sineComponent = sinBody + subOct * p.subOctave * 0.3f;

            // === REESE BASS: 3 saws detuned con LFO wobble ===
            float reeseL = 0.0f, reeseR = 0.0f;
            if (p.reeseMix > 0.01f)
            {
                // Detune base en cents (rango amplio para Reese real)
                float detuneCents = p.reeseDetune * 80.0f;

                // LFO modula el detune → "wobble" / movimiento del Reese
                float wobbleRate = 0.8f + p.reeseDetune * 1.5f;  // 0.8-2.3 Hz
                float wobbleLfo = std::sin (dsputil::TWO_PI * wobbleRate * t + lfoPhaseOffset);
                float detuneModulated = detuneCents * (1.0f + wobbleLfo * 0.25f);

                // 3 frecuencias: centro, +detune, -detune
                float freq1 = currentFreq;  // centro
                float freq2 = currentFreq * std::pow (2.0f, detuneModulated / 1200.0f);
                float freq3 = currentFreq * std::pow (2.0f, -detuneModulated / 1200.0f);

                reeseSaw1.setFrequency (freq1, sr);
                reeseSaw2.setFrequency (freq2, sr);
                reeseSaw3.setFrequency (freq3, sr);

                float saw1 = reeseSaw1.next (OscillatorType::Saw);
                float saw2 = reeseSaw2.next (OscillatorType::Saw);
                float saw3 = reeseSaw3.next (OscillatorType::Saw);

                // Stereo: saw centro en mono, saw2 más a L, saw3 más a R
                float stereoAmt = std::min (0.4f, p.reeseDetune * 0.5f);
                float midGain = 0.45f;
                float sideGain = 0.35f;

                reeseL = saw1 * midGain
                       + saw2 * (sideGain + stereoAmt * 0.3f)
                       + saw3 * (sideGain - stereoAmt * 0.15f);

                reeseR = saw1 * midGain
                       + saw2 * (sideGain - stereoAmt * 0.15f)
                       + saw3 * (sideGain + stereoAmt * 0.3f);

                // === REESE FILTER: LP con LFO → "growl" ===
                float filterLfoRate = 1.2f + p.reeseDetune * 2.0f;  // 1.2-3.2 Hz
                float filterLfo = std::sin (dsputil::TWO_PI * filterLfoRate * t
                                            + lfoPhaseOffset * 1.7f);
                float reeseFilterFreq = p.base.filterCutoff
                                      * (0.6f + 0.4f * (0.5f + filterLfo * 0.5f));
                reeseFilterFreq = std::max (200.0f, std::min (reeseFilterFreq, sr * 0.48f));
                float reeseQ = 0.15f + p.reeseDetune * 0.20f;
                reeseFilterL.setParameters (reeseFilterFreq, reeseQ,
                                            FilterType::LowPass, sr);
                reeseFilterR.setParameters (reeseFilterFreq, reeseQ,
                                            FilterType::LowPass, sr);

                reeseL = reeseFilterL.process (reeseL);
                reeseR = reeseFilterR.process (reeseR);
            }

            // === MIX: sine vs reese ===
            float bodyL, bodyR;
            if (p.reeseMix > 0.01f)
            {
                float sineMix = 1.0f - p.reeseMix;
                bodyL = sineComponent * sineMix + reeseL * p.reeseMix;
                bodyR = sineComponent * sineMix + reeseR * p.reeseMix;
            }
            else
            {
                bodyL = sineComponent;
                bodyR = sineComponent;
            }

            // === SATURATION (sutil — solo warmth, no distorsión destructiva) ===
            float totalDrive = std::min (0.30f, p.saturation * 0.5f + p.harmonics * 0.2f);
            if (totalDrive > 0.01f)
            {
                // Tape para todo — warmth suave sin destruir la señal
                // Tube es demasiado agresivo para bajos
                bodyL = saturator.process (bodyL, totalDrive, dsputil::SaturationMode::Tape);
                bodyR = saturator.process (bodyR, totalDrive, dsputil::SaturationMode::Tape);
            }

            // === FILTER ENVELOPE (donk) ===
            if (p.filterEnvAmt > 0.01f)
            {
                float filterEnv = std::exp (-t / 0.06f);
                float envCutoff = envFilterBaseFreq
                                + filterEnv * p.filterEnvAmt * 3000.0f;
                envCutoff = std::min (envCutoff, sr * 0.48f);
                float envQ = std::min (0.35f, 0.15f + p.filterEnvAmt * 0.20f);
                envFilter.setParameters (envCutoff, envQ, FilterType::LowPass, sr);
                bodyL = envFilter.process (bodyL);
                // Reuse same filter for mono consistency on filter env
                bodyR = bodyL;
            }

            // === PUNCH ===
            float punchEnv = std::exp (-t / 0.003f);
            float punch = punchBP.process (sinBody) * punchEnv * p.punchiness * 0.25f;

            // === AMPLITUDE ENVELOPE ===
            float ampEnv;
            if (t < attackSec)
            {
                ampEnv = 0.5f - 0.5f * std::cos (dsputil::PI * t / attackSec);
            }
            else
            {
                float dt = t - attackSec;
                float bodyEnvVal = p.sustainLevel
                              + (1.0f - p.sustainLevel) * std::exp (-dt / decaySec);
                float tailEnv = std::exp (-dt / tailSec);
                ampEnv = bodyEnvVal * tailEnv;
            }

            if (ampEnv < 0.001f && t > attackSec + decaySec)
                break;

            float sampleL = (bodyL * 0.75f + punch) * ampEnv;
            float sampleR = (bodyR * 0.75f + punch) * ampEnv;

            // === BODY FILTER ===
            sampleL = bodyFilter.process (sampleL);
            // Para el canal R, solo aplicamos body filter si no es Reese
            // (Reese ya tiene su propio filtro stereo)
            if (p.reeseMix < 0.3f)
                sampleR = sampleL;  // mono para 808 puro
            else
                sampleR = bodyFilter.process (sampleR);

            // === DC BLOCK ===
            sampleL = dcBlockL.process (sampleL);
            sampleR = dcBlockR.process (sampleR);

            sampleL *= p.base.volume;
            sampleR *= p.base.volume;

            // Pan
            float panL = std::cos (p.base.pan * dsputil::PI * 0.5f);
            float panR = std::sin (p.base.pan * dsputil::PI * 0.5f);

            buffer.setSample (0, i, sampleL * panL);
            buffer.setSample (1, i, sampleR * panR);
        }

        synthutil::applyFadeOut (buffer, (int)(0.02f * sr));
        return buffer;
    }
};

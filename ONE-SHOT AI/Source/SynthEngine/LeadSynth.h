#pragma once

#include <JuceHeader.h>
#include "../Params/InstrumentParams.h"
#include "../DSP/Oscillator.h"
#include "../DSP/Envelope.h"
#include "../DSP/SynthFilter.h"
#include "../DSP/Saturator.h"
#include "../DSP/DCBlocker.h"
#include "SynthUtils.h"

// Lead synth profesional — UPGRADE
//
// Mejoras sobre la version anterior:
//   — Filter envelope: filterEnvAmt controla apertura/cierre del filtro
//   — PWM real: pulseWidth funcional via PolyBLEP dual-saw
//   — Wave morph: waveformMix como crossfade continuo saw ↔ square/PWM
//   — Sub-oscillator: sine a -1 octava para cuerpo/peso
//   — Ataque curvado (exponencial, no lineal)
//   — Saturacion Tube (even harmonics = calidez analoga)
//   — DC blocking

class LeadSynth
{
public:
    juce::AudioBuffer<float> generate (const LeadParams& p, double sampleRate, unsigned int seed)
    {
        float sr = (float) sampleRate;

        float sustainHold = 0.3f + p.base.sustain * 0.5f;
        float duration = p.base.attack + p.base.decay
                       + sustainHold + p.base.release + 0.1f;
        int numSamples = synthutil::durationInSamples (duration, sampleRate);

        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        int numVoices = std::max (1, std::min (8, (int) p.unisonVoices));

        std::vector<dsputil::Oscillator> oscs ((size_t) numVoices);
        for (int v = 0; v < numVoices; ++v)
            oscs[(size_t) v].reset ((float) v / (float) numVoices);

        // Sub-oscillator
        dsputil::Oscillator subOsc;
        subOsc.reset (0.0f);

        dsputil::Oscillator vibratoLFO;
        vibratoLFO.reset();

        // Amp envelope con ataque curvado
        dsputil::ADSREnvelope ampEnv;
        ampEnv.setParameters (p.base.attack, p.base.decay, p.base.sustain,
                              p.base.release, sr);
        ampEnv.setAttackCurve (0.6f);
        ampEnv.trigger();

        // Filter envelope (para filterEnvAmt)
        dsputil::ADSREnvelope filterEnv;
        filterEnv.setParameters (p.base.attack * 0.3f, p.base.decay * 1.5f,
                                 0.2f, p.base.release * 0.5f, sr);
        filterEnv.setAttackCurve (0.5f);
        filterEnv.trigger();

        int releaseAtSample = synthutil::durationInSamples (
            p.base.attack + p.base.decay + sustainHold, sampleRate);

        // TWO separate filters for proper stereo
        float filterBase = 200.0f + p.brightness * (p.base.filterCutoff - 200.0f);
        dsputil::SVFilter filterL, filterR;

        float baseFreq = synthutil::midiToFreq (p.base.pitchBase);

        std::vector<float> voicePans ((size_t) numVoices);
        for (int v = 0; v < numVoices; ++v)
        {
            if (numVoices > 1)
                voicePans[(size_t) v] = 0.3f + 0.4f
                    * ((float) v / (float)(numVoices - 1));
            else
                voicePans[0] = 0.5f;
        }

        dsputil::Saturator saturator;
        dsputil::DCBlocker dcL, dcR;

        for (int i = 0; i < numSamples; ++i)
        {
            float t = (float) i / sr;

            if (i == releaseAtSample)
            {
                ampEnv.startRelease();
                filterEnv.startRelease();
            }

            float env = ampEnv.next();
            float fEnv = filterEnv.next();
            if (! ampEnv.isActive() && i > releaseAtSample)
                break;

            // === FILTER ENVELOPE: modula cutoff (rango controlado) ===
            float filterCutoff = filterBase
                               + fEnv * p.filterEnvAmt * 4500.0f;
            filterCutoff = synthutil::clamp (filterCutoff, 80.0f, sr * 0.49f);
            filterL.setParameters (filterCutoff, p.base.filterResonance,
                                   FilterType::LowPass, sr);
            filterR.setParameters (filterCutoff, p.base.filterResonance,
                                   FilterType::LowPass, sr);

            // === VIBRATO ===
            vibratoLFO.setFrequency (p.vibratoRate, sr);
            float vibrato = vibratoLFO.next (OscillatorType::Sine)
                          * p.vibratoDepth * 2.0f;
            float pitchMod = std::pow (2.0f, vibrato / 12.0f);

            // === UNISON VOICES con wave morph y PWM ===
            float sumL = 0.0f, sumR = 0.0f;
            for (int v = 0; v < numVoices; ++v)
            {
                float detuneOffset = 0.0f;
                if (numVoices > 1)
                {
                    float spread = ((float) v / (float)(numVoices - 1))
                                 * 2.0f - 1.0f;
                    detuneOffset = spread * p.oscDetune * 5.0f;
                }

                float voiceFreq = baseFreq * pitchMod
                                * std::pow (2.0f, detuneOffset / 12.0f);
                oscs[(size_t) v].setFrequency (voiceFreq, sr);

                // Wave morph: continuo saw ↔ PWM square
                float saw = oscs[(size_t) v].next (OscillatorType::Saw);
                float pwm = oscs[(size_t) v].nextPWM (p.pulseWidth);
                float voiceSample = saw * (1.0f - p.waveformMix)
                                  + pwm * p.waveformMix;

                float vL, vR;
                synthutil::panMonoToStereo (voiceSample,
                                            voicePans[(size_t) v], vL, vR);
                sumL += vL;
                sumR += vR;
            }
            float voiceNorm = 1.0f / std::sqrt ((float) numVoices);
            sumL *= voiceNorm;
            sumR *= voiceNorm;

            // === SUB OSCILLATOR ===
            if (p.subOscLevel > 0.01f)
            {
                subOsc.setFrequency (baseFreq * 0.5f, sr);
                float sub = subOsc.next (OscillatorType::Sine);
                sumL += sub * p.subOscLevel * 0.4f;
                sumR += sub * p.subOscLevel * 0.4f;
            }

            // === ENVELOPE ===
            sumL *= env;
            sumR *= env;

            // === SEPARATE STEREO FILTERING ===
            sumL = filterL.process (sumL);
            sumR = filterR.process (sumR);

            // === TUBE SATURATION (even harmonics = calidez) ===
            if (p.base.saturationAmount > 0.01f)
            {
                sumL = saturator.process (sumL, p.base.saturationAmount,
                                          dsputil::SaturationMode::Tube);
                sumR = saturator.process (sumR, p.base.saturationAmount,
                                          dsputil::SaturationMode::Tube);
            }

            sumL *= p.base.volume;
            sumR *= p.base.volume;

            // DC block
            sumL = dcL.process (sumL);
            sumR = dcR.process (sumR);

            float panL = std::cos (p.base.pan * dsputil::PI * 0.5f);
            float panR = std::sin (p.base.pan * dsputil::PI * 0.5f);
            buffer.setSample (0, i, sumL * panL);
            buffer.setSample (1, i, sumR * panR);
        }

        synthutil::applyFadeOut (buffer, (int)(0.015f * sr));
        return buffer;
    }
};

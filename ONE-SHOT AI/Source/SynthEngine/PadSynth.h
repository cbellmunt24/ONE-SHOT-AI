#pragma once

#include <JuceHeader.h>
#include "../Params/InstrumentParams.h"
#include "../DSP/Oscillator.h"
#include "../DSP/Envelope.h"
#include "../DSP/NoiseGenerator.h"
#include "../DSP/SynthFilter.h"
#include "../DSP/DelayLine.h"
#include "../DSP/DCBlocker.h"
#include "SynthUtils.h"

// Pad synth profesional nivel Splice — REWRITE COMPLETO
//
// Arquitectura multi-layer:
//   Layer 1 — SUB:  Sine pura a la fundamental (peso, profundidad)
//   Layer 2 — BODY: Saws unison PolyBLEP (6-16 voces) → warmth LP modulado
//   Layer 3 — AIR:  Brown/pink noise filtrado suave (respira, textura aerea)
//
// Features pro:
//   — Chorus real via delay modulado (no fake amplitude mod)
//   — Warmth filter con LFO lento → movimiento organico
//   — Stereo decorrelation: voces L/R con fases y drift independientes
//   — Ataque curvado exponencial → sin transiente digital
//   — 4-pole warmth filter para corte mas gordo
//   — DC blocking post-sintesis

class PadSynth
{
public:
    juce::AudioBuffer<float> generate (const PadParams& p, double sampleRate, unsigned int seed)
    {
        const float sr = (float) sampleRate;

        float sustainHold = 1.0f + p.base.sustain * 2.0f;
        float duration = p.base.attack + p.base.decay
                       + sustainHold + p.base.release + 0.15f;
        int numSamples = synthutil::durationInSamples (duration, sampleRate);

        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        int numVoices = std::max (2, std::min (16, (int) p.unisonVoices));
        float baseFreq = synthutil::midiToFreq (p.base.pitchBase);

        // ============================================================
        // OSCILLATORS
        // ============================================================

        // Body: PolyBLEP saws con unison
        std::vector<dsputil::Oscillator> oscs ((size_t) numVoices);

        // Sub: sine pura a la fundamental
        dsputil::Oscillator subOsc;
        subOsc.reset (0.0f);

        // Noise generators para air layer y drift
        dsputil::NoiseGenerator airNoise;
        airNoise.setSeed (seed);

        dsputil::NoiseGenerator driftNoise;
        driftNoise.setSeed (seed + 1);

        // ============================================================
        // VOICE SETUP: detune, pan, phase offsets
        // ============================================================

        std::vector<float> detuneOffsets ((size_t) numVoices);
        std::vector<float> voicePans ((size_t) numVoices);
        std::vector<float> driftPhases ((size_t) numVoices);

        for (int v = 0; v < numVoices; ++v)
        {
            // Fases distribuidas uniformemente → evita cancellation
            oscs[(size_t) v].reset ((float) v / (float) numVoices);

            // Detune simetrico: voces spread desde -detuneSpread a +detuneSpread
            if (numVoices > 1)
            {
                float spread = ((float) v / (float)(numVoices - 1)) * 2.0f - 1.0f;
                detuneOffsets[(size_t) v] = spread * p.detuneSpread * 2.0f;
            }
            else
            {
                detuneOffsets[0] = 0.0f;
            }

            // Pan: voces distribuidas en el campo estereo
            if (numVoices > 1)
                voicePans[(size_t) v] = 0.5f
                    + ((float) v / (float)(numVoices - 1) - 0.5f) * p.stereoWidth;
            else
                voicePans[0] = 0.5f;

            // Drift phase: cada voz tiene un offset de drift diferente
            driftPhases[(size_t) v] = driftNoise.next() * dsputil::TWO_PI;
        }

        // ============================================================
        // ENVELOPES
        // ============================================================

        dsputil::ADSREnvelope ampEnv;
        ampEnv.setParameters (p.base.attack, p.base.decay,
                              p.base.sustain, p.base.release, sr);
        ampEnv.setAttackCurve (0.95f);  // Ataque ultra-suave (exponencial profundo)
        ampEnv.trigger();

        int releaseAtSample = synthutil::durationInSamples (
            p.base.attack + p.base.decay + sustainHold, sampleRate);

        // ============================================================
        // WARMTH FILTER (4-pole para corte mas gordo)
        // warmth=0 → 4500Hz (bright pero suave)
        // warmth=1 → 500Hz (muy calido y redondo)
        // Resonancia mínima para evitar picos punzantes
        // ============================================================

        float warmthBase = 500.0f + (1.0f - p.warmth) * 4000.0f;
        dsputil::SVFilter4Pole warmthL, warmthR;
        warmthL.setParameters (warmthBase, 0.05f, FilterType::LowPass, sr);
        warmthR.setParameters (warmthBase, 0.05f, FilterType::LowPass, sr);

        // Main filter (with sweep)
        dsputil::SVFilter filterL, filterR;

        // ============================================================
        // REAL CHORUS: delay modulado con LFO
        // ============================================================

        const float chorusBaseDelay = 8.0f;  // ms
        const float chorusModDepth = p.chorusAmount * 3.5f;  // ms
        int chorusMaxSamples = (int)((chorusBaseDelay + chorusModDepth + 2.0f) * 0.001f * sr) + 4;

        dsputil::DelayLine chorusDelayL, chorusDelayR;
        chorusDelayL.setSize (chorusMaxSamples);
        chorusDelayR.setSize (chorusMaxSamples);
        chorusDelayL.clear();
        chorusDelayR.clear();

        // ============================================================
        // AIR LAYER FILTER: LP suave para brown/pink noise
        // ============================================================

        dsputil::SVFilter airFilterL, airFilterR;
        float airCutoff = 1200.0f + (1.0f - p.warmth) * 3000.0f;
        airFilterL.setParameters (airCutoff, 0.03f, FilterType::LowPass, sr);
        airFilterR.setParameters (airCutoff, 0.03f, FilterType::LowPass, sr);

        // DC blockers
        dsputil::DCBlocker dcL, dcR;

        // ============================================================
        // RENDER LOOP
        // ============================================================

        for (int i = 0; i < numSamples; ++i)
        {
            float t = (float) i / sr;

            if (i == releaseAtSample)
                ampEnv.startRelease();

            float env = ampEnv.next();
            if (! ampEnv.isActive() && i > releaseAtSample)
                break;

            // === WARMTH FILTER MODULATION (LFO lento → movimiento suave) ===
            float warmthMod = std::sin (dsputil::TWO_PI * p.evolutionRate * t)
                            * p.filterSweep * 0.25f;
            float warmthCutoff = warmthBase * (1.0f + warmthMod);
            warmthCutoff = std::max (200.0f, std::min (warmthCutoff, sr * 0.48f));
            warmthL.setParameters (warmthCutoff, 0.05f, FilterType::LowPass, sr);
            warmthR.setParameters (warmthCutoff, 0.05f, FilterType::LowPass, sr);

            // === MAIN FILTER SWEEP ===
            float sweepMod = std::sin (dsputil::TWO_PI * p.evolutionRate * 1.7f * t)
                           * p.filterSweep;
            float filterFreq = synthutil::clamp (
                p.base.filterCutoff * (1.0f + sweepMod * 0.5f), 20.0f, sr * 0.49f);
            filterL.setParameters (filterFreq, p.base.filterResonance,
                                   p.base.filterType, sr);
            filterR.setParameters (filterFreq, p.base.filterResonance,
                                   p.base.filterType, sr);

            // ================================================================
            // LAYER 2 — BODY: Unison saws
            // ================================================================

            float sumL = 0.0f, sumR = 0.0f;

            for (int v = 0; v < numVoices; ++v)
            {
                // Analog drift: cada voz fluctua independientemente
                float drift = std::sin (dsputil::TWO_PI * p.driftRate * t
                                        + driftPhases[(size_t) v]);
                float driftCents = drift * p.detuneSpread * 5.0f;

                float voiceFreq = baseFreq * std::pow (2.0f,
                    (detuneOffsets[(size_t) v] + driftCents * 0.01f) / 12.0f);

                oscs[(size_t) v].setFrequency (voiceFreq, sr);

                // SAW wave con PolyBLEP anti-aliasing
                float voiceSample = oscs[(size_t) v].next (OscillatorType::Saw);

                float vL, vR;
                synthutil::panMonoToStereo (voiceSample,
                                            voicePans[(size_t) v], vL, vR);
                sumL += vL;
                sumR += vR;
            }

            // Normalizar voces
            float voiceNorm = 1.0f / std::sqrt ((float) numVoices);
            sumL *= voiceNorm;
            sumR *= voiceNorm;

            // WARMTH FILTER: 4-pole LP → redondea las saws en un pad calido
            sumL = warmthL.process (sumL);
            sumR = warmthR.process (sumR);

            // ================================================================
            // LAYER 1 — SUB: Sine pura a la fundamental
            // ================================================================

            subOsc.setFrequency (baseFreq, sr);
            float subSample = subOsc.next (OscillatorType::Sine);
            // Sub: mono centrado, sin efectos
            sumL += subSample * p.subLevel * 0.5f;
            sumR += subSample * p.subLevel * 0.5f;

            // ================================================================
            // LAYER 3 — AIR: Brown/pink noise filtrado suave
            // ================================================================

            if (p.airAmount > 0.005f)
            {
                // Brown noise para textura aerea suave
                float airRaw = airNoise.nextBrown() * 0.6f
                             + airNoise.nextPink() * 0.4f;

                // Stereo: ruidos decorrelados
                float airL = airFilterL.process (airRaw);
                float airR = airFilterR.process (airNoise.nextBrown() * 0.5f
                                                 + airNoise.nextPink() * 0.5f);

                sumL += airL * p.airAmount * 0.25f;
                sumR += airR * p.airAmount * 0.25f;
            }

            // ================================================================
            // REAL CHORUS: delay modulado
            // ================================================================

            if (p.chorusAmount > 0.01f)
            {
                chorusDelayL.write (sumL);
                chorusDelayR.write (sumR);

                // LFOs desfasados para L y R
                float chorusLfoL = std::sin (dsputil::TWO_PI * 0.9f * t);
                float chorusLfoR = std::sin (dsputil::TWO_PI * 1.1f * t + 1.2f);

                float chorusMsL = chorusBaseDelay + chorusLfoL * chorusModDepth;
                float chorusMsR = chorusBaseDelay + chorusLfoR * chorusModDepth;

                float chorusSampL = chorusMsL * 0.001f * sr;
                float chorusSampR = chorusMsR * 0.001f * sr;

                float chorusL = chorusDelayL.readHermite (chorusSampL);
                float chorusR = chorusDelayR.readHermite (chorusSampR);

                float wet = p.chorusAmount * 0.5f;
                sumL = sumL * (1.0f - wet) + chorusL * wet;
                sumR = sumR * (1.0f - wet) + chorusR * wet;
            }

            // ================================================================
            // MAIN FILTER + ENVELOPE + OUTPUT
            // ================================================================

            sumL = filterL.process (sumL);
            sumR = filterR.process (sumR);

            // Envelope
            sumL *= env * p.base.volume;
            sumR *= env * p.base.volume;

            // DC block
            sumL = dcL.process (sumL);
            sumR = dcR.process (sumR);

            // Pan
            float panL = std::cos (p.base.pan * dsputil::PI * 0.5f);
            float panR = std::sin (p.base.pan * dsputil::PI * 0.5f);
            buffer.setSample (0, i, sumL * panL);
            buffer.setSample (1, i, sumR * panR);
        }

        synthutil::applyFadeOut (buffer, (int)(0.04f * sr));
        return buffer;
    }
};

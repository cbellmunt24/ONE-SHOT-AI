#pragma once

#include <JuceHeader.h>
#include "EffectParams.h"
#include "../DSP/DelayLine.h"
#include "../DSP/SynthFilter.h"
#include "../DSP/DSPConstants.h"

// Reverb FDN (Feedback Delay Network) — REWRITE COMPLETO
//
// 8 delay lines con Hadamard mixing para difusion densa y natural.
// Early reflections separadas (6 taps).
// Modulacion sutil de delay times para evitar metallic ringing.
// Pre-delay con interpolacion.
// Damping one-pole por linea.

class ReverbEffect
{
public:
    void process (juce::AudioBuffer<float>& buffer, const ReverbParams& p, float sampleRate)
    {
        if (! p.enabled || p.mix < 0.001f)
            return;

        const int numSamples = buffer.getNumSamples();
        const float wet = p.mix;
        const float dry = 1.0f - wet;
        const float sr = sampleRate;

        // ============================================================
        // INIT DELAY LINES
        // ============================================================

        // Base delay times en ms (primos para maxima densidad)
        static constexpr float baseDelayTimesMs[8] = {
            29.3f, 36.7f, 41.2f, 43.8f, 47.1f, 51.3f, 55.8f, 59.4f
        };

        float sizeScale = 0.5f + p.size * 1.2f; // 0.5x .. 1.7x

        dsputil::DelayLine fdnLines[8];
        dsputil::OnePole dampFilters[8];
        float feedbacks[8];

        float baseFeedback = 0.65f + p.decay * 0.30f; // 0.65 .. 0.95

        for (int i = 0; i < 8; ++i)
        {
            float delayMs = baseDelayTimesMs[i] * sizeScale;
            int delaySamples = std::max (1, (int)(delayMs * 0.001f * sr));
            fdnLines[i].setSize (delaySamples + 4);
            fdnLines[i].clear();

            // Damping: LP filter per line
            dampFilters[i].setLowPass (2000.0f + (1.0f - p.damping) * 14000.0f, sr);

            // Feedback ligeramente diferente por linea para variedad
            feedbacks[i] = baseFeedback * (0.96f + 0.08f * ((float) i / 7.0f));
        }

        // FDN state
        float fdnState[8] = {};

        // ============================================================
        // EARLY REFLECTIONS (6 taps)
        // ============================================================

        static constexpr float earlyTimesMs[6] = {
            3.2f, 7.4f, 11.5f, 17.3f, 22.1f, 25.8f
        };
        static constexpr float earlyGains[6] = {
            0.85f, 0.72f, 0.60f, 0.48f, 0.38f, 0.30f
        };
        // Panning L/R para early reflections
        static constexpr float earlyPanL[6] = {
            0.9f, 0.3f, 0.7f, 0.4f, 0.8f, 0.2f
        };

        dsputil::DelayLine earlyDelayL, earlyDelayR;
        int maxEarlySamples = (int)(30.0f * 0.001f * sr * sizeScale) + 4;
        earlyDelayL.setSize (maxEarlySamples);
        earlyDelayR.setSize (maxEarlySamples);
        earlyDelayL.clear();
        earlyDelayR.clear();

        // ============================================================
        // PRE-DELAY
        // ============================================================

        int predelaySamples = std::max (1, (int)(p.predelay * sr));
        dsputil::DelayLine predelayLine;
        predelayLine.setSize (predelaySamples + 2);
        predelayLine.clear();

        // ============================================================
        // PROCESS
        // ============================================================

        auto* dataL = buffer.getWritePointer (0);
        auto* dataR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            float inL = dataL[i];
            float inR = dataR ? dataR[i] : inL;
            float mono = (inL + inR) * 0.5f;

            // Pre-delay
            predelayLine.write (mono);
            float pdSignal = predelayLine.read (predelaySamples);

            // === EARLY REFLECTIONS ===
            earlyDelayL.write (pdSignal);
            earlyDelayR.write (pdSignal);

            float earlyL = 0.0f, earlyR = 0.0f;
            for (int e = 0; e < 6; ++e)
            {
                float eDelay = earlyTimesMs[e] * 0.001f * sr * sizeScale;
                float eSample = earlyDelayL.readHermite (eDelay);
                earlyL += eSample * earlyGains[e] * earlyPanL[e];
                earlyR += eSample * earlyGains[e] * (1.0f - earlyPanL[e]);
            }
            earlyL *= 0.25f;
            earlyR *= 0.25f;

            // === FDN: read all lines ===
            float reads[8];
            for (int k = 0; k < 8; ++k)
            {
                float delayMs = baseDelayTimesMs[k] * sizeScale;
                // Modulacion sutil para evitar metallic ringing
                float modMs = 0.3f * std::sin (dsputil::TWO_PI * (0.1f + 0.05f * k) * (float) i / sr);
                float delaySamp = (delayMs + modMs) * 0.001f * sr;
                delaySamp = std::max (1.0f, delaySamp);
                reads[k] = fdnLines[k].readHermite (delaySamp);
            }

            // === HADAMARD MIXING (8x8 normalizado) ===
            // Hadamard 8x8: cada output es suma/resta de todos los inputs
            float mixed[8];
            float inv = 1.0f / 2.828f; // 1/sqrt(8)

            // Hadamard butterfly en 3 etapas
            float a0 = reads[0] + reads[1];
            float a1 = reads[0] - reads[1];
            float a2 = reads[2] + reads[3];
            float a3 = reads[2] - reads[3];
            float a4 = reads[4] + reads[5];
            float a5 = reads[4] - reads[5];
            float a6 = reads[6] + reads[7];
            float a7 = reads[6] - reads[7];

            float b0 = a0 + a2;
            float b1 = a1 + a3;
            float b2 = a0 - a2;
            float b3 = a1 - a3;
            float b4 = a4 + a6;
            float b5 = a5 + a7;
            float b6 = a4 - a6;
            float b7 = a5 - a7;

            mixed[0] = (b0 + b4) * inv;
            mixed[1] = (b1 + b5) * inv;
            mixed[2] = (b2 + b6) * inv;
            mixed[3] = (b3 + b7) * inv;
            mixed[4] = (b0 - b4) * inv;
            mixed[5] = (b1 - b5) * inv;
            mixed[6] = (b2 - b6) * inv;
            mixed[7] = (b3 - b7) * inv;

            // === DAMPING + FEEDBACK + WRITE ===
            float input = pdSignal * 0.25f;
            for (int k = 0; k < 8; ++k)
            {
                float damped = dampFilters[k].process (mixed[k]);
                float toWrite = input + damped * feedbacks[k];
                // Soft limit para estabilidad
                if (toWrite > 1.5f) toWrite = 1.5f;
                if (toWrite < -1.5f) toWrite = -1.5f;
                fdnLines[k].write (toWrite);
                fdnState[k] = damped;
            }

            // === OUTPUT: mezcla decorrelada L/R ===
            float lateL = (fdnState[0] + fdnState[2] + fdnState[4] + fdnState[6]) * 0.25f;
            float lateR = (fdnState[1] + fdnState[3] + fdnState[5] + fdnState[7]) * 0.25f;

            // Combinar early + late reflections
            float wetL = earlyL * 0.4f + lateL * 0.6f;
            float wetR = earlyR * 0.4f + lateR * 0.6f;

            dataL[i] = inL * dry + wetL * wet;
            if (dataR)
                dataR[i] = inR * dry + wetR * wet;
        }
    }
};

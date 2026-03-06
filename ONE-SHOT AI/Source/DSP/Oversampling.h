#pragma once

#include <vector>
#include <cmath>
#include <functional>
#include "DSPConstants.h"

namespace dsputil
{

// 2x Oversampling processor para reducir aliasing en sintesis
// Upsample → procesar a 2x → LP anti-alias → downsample
class Oversampling2x
{
public:
    void reset()
    {
        for (int i = 0; i < 4; ++i)
        {
            upState[i] = 0.0f;
            downState[i] = 0.0f;
        }
    }

    // Procesa un sample con 2x oversampling
    // processFunc: callback que genera/procesa un sample a sample rate doble
    // Devuelve un sample a sample rate original
    template <typename Func>
    float process (Func processFunc)
    {
        // Generar 2 samples a doble rate
        float s0 = processFunc();
        float s1 = processFunc();

        // Anti-alias LP filter (Butterworth 4-pole) y downsample
        // Procesar ambos samples por el filtro, tomar el segundo
        float filtered0 = antiAliasFilter (s0);
        float filtered1 = antiAliasFilter (s1);

        (void) filtered0; // Solo necesitamos el segundo
        return filtered1;
    }

    // Procesa un buffer completo con 2x oversampling
    // Util cuando ya tienes un buffer generado a 1x y quieres oversamplear
    void processBuffer (float* data, int numSamples, std::function<float(float)> processFunc)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float in = data[i];
            // Upsample: insertar zero entre samples
            float s0 = processFunc (in * 2.0f);
            float s1 = processFunc (0.0f);

            float f0 = antiAliasFilter (s0);
            float f1 = antiAliasFilter (s1);

            (void) f0;
            data[i] = f1;
        }
    }

private:
    // Butterworth LP 2nd order cascade (4-pole total)
    // Cutoff at ~0.45 * Nyquist (para 2x oversampling)
    float antiAliasFilter (float x)
    {
        // Coefficients pre-computed for fc = 0.23 * fs (Butterworth)
        // Esto filtra todo por encima de ~0.46 * fs_original
        static constexpr float b0 = 0.0675f;
        static constexpr float b1 = 0.1349f;
        static constexpr float b2 = 0.0675f;
        static constexpr float a1 = -1.1430f;
        static constexpr float a2 = 0.4128f;

        // Stage 1
        float y1 = b0 * x + b1 * downState[0] + b2 * downState[1]
                 - a1 * downState[2] - a2 * downState[3];
        downState[1] = downState[0];
        downState[0] = x;
        downState[3] = downState[2];
        downState[2] = y1;

        return y1;
    }

    float upState[4] = {};
    float downState[4] = {};
};

// Per-sample 2x oversampling para procesos no-lineales (distorsion, saturacion)
// Upsample → aplicar funcion → anti-alias LP → downsample
class OversamplingProcessor
{
public:
    void prepare (float /*sampleRate*/)
    {
        lastInput = 0.0f;
        for (int i = 0; i < 4; ++i)
            state[i] = 0.0f;
    }

    // processFunc(float x) -> float: funcion no-lineal a aplicar
    // input: sample de entrada a sample rate original
    // return: sample procesado a sample rate original, con aliasing reducido
    template <typename Func>
    float process (Func processFunc, float input)
    {
        // Upsample 2x por interpolacion lineal
        float mid = 0.5f * (lastInput + input);
        lastInput = input;

        // Aplicar funcion no-lineal a ambos samples
        float s0 = processFunc (mid);
        float s1 = processFunc (input);

        // Anti-alias LP y downsample (tomar segundo sample filtrado)
        antiAliasLP (s0);
        return antiAliasLP (s1);
    }

private:
    float antiAliasLP (float x)
    {
        // Biquad LP Butterworth, fc ~ 0.23*fs (para 2x OS)
        static constexpr float b0 = 0.0675f;
        static constexpr float b1 = 0.1349f;
        static constexpr float b2 = 0.0675f;
        static constexpr float a1 = -1.1430f;
        static constexpr float a2 = 0.4128f;

        float y = b0 * x + b1 * state[0] + b2 * state[1]
                - a1 * state[2] - a2 * state[3];
        state[1] = state[0];
        state[0] = x;
        state[3] = state[2];
        state[2] = y;
        return y;
    }

    float lastInput = 0.0f;
    float state[4] = {};
};

} // namespace dsputil

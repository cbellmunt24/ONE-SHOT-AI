#pragma once

#include <random>
#include <cmath>

namespace dsputil
{

class NoiseGenerator
{
public:
    void setSeed (unsigned int seed)
    {
        rng.seed (seed);
        // Reset pink noise state
        for (int i = 0; i < 16; ++i)
            pinkRows[i] = 0.0f;
        pinkRunningSum = 0.0f;
        pinkIndex = 0;
        brownState = 0.0f;
    }

    // Ruido blanco uniforme [-1, 1]
    float next() { return dist (rng); }

    // Ruido rosa (1/f) via Voss-McCartney algorithm - 16 octave bands
    // Espectro que decae 3dB/octava → percusion natural, snares, hihats
    float nextPink()
    {
        // Voss-McCartney: cada fila se actualiza a frecuencias distintas (1/2^n)
        int lastIndex = pinkIndex;
        ++pinkIndex;

        // Determinar que filas actualizar via trailing zeros del incremento
        int diff = lastIndex ^ pinkIndex;

        for (int i = 0; i < numPinkRows; ++i)
        {
            if (diff & (1 << i))
            {
                pinkRunningSum -= pinkRows[i];
                pinkRows[i] = dist (rng) * pinkRowScale;
                pinkRunningSum += pinkRows[i];
            }
        }

        // Agregar ruido blanco de alta frecuencia
        float white = dist (rng) * pinkRowScale;
        float pink = (pinkRunningSum + white) * pinkNormalize;

        return pink;
    }

    // Ruido brown/Brownian (random walk integrado) → movimiento suave
    // Ideal para pads, texturas, modulacion lenta
    float nextBrown()
    {
        brownState += dist (rng) * 0.125f;

        // Limitar para evitar drift excesivo
        if (brownState > 1.0f) brownState = 1.0f;
        if (brownState < -1.0f) brownState = -1.0f;

        // Leaky integrator para estabilidad
        brownState *= 0.998f;

        return brownState;
    }

    // Ruido filtrado: blend entre white, pink y brown segun parametro color
    // color: 0 = brown (oscuro), 0.5 = pink (natural), 1.0 = white (brillante)
    float nextColored (float color)
    {
        if (color < 0.5f)
        {
            float t = color * 2.0f; // 0..1
            return nextBrown() * (1.0f - t) + nextPink() * t;
        }
        else
        {
            float t = (color - 0.5f) * 2.0f; // 0..1
            return nextPink() * (1.0f - t) + next() * t;
        }
    }

private:
    std::mt19937 rng { 42 };
    std::uniform_real_distribution<float> dist { -1.0f, 1.0f };

    // Pink noise state (Voss-McCartney)
    static constexpr int numPinkRows = 12;
    float pinkRows[16] = {};
    float pinkRunningSum = 0.0f;
    int pinkIndex = 0;
    static constexpr float pinkRowScale = 1.0f / (float) numPinkRows;
    static constexpr float pinkNormalize = 1.0f / ((float) numPinkRows + 1.0f) * 3.5f;

    // Brown noise state
    float brownState = 0.0f;
};

} // namespace dsputil

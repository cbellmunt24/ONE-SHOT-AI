#pragma once

#include <cmath>
#include <random>
#include "DSPConstants.h"
#include "../Params/SynthEnums.h"

namespace dsputil
{

class Oscillator
{
public:
    void reset (float startPhase = 0.0f)
    {
        phase = startPhase;
        subPhase = startPhase * 0.5f;
        lastSawForTri = 0.0f;
        triIntegrator = 0.0f;
    }

    void setFrequency (float freq, float sampleRate)
    {
        increment = freq / sampleRate;
    }

    void setSeed (unsigned int seed) { rng.seed (seed); }

    float next (OscillatorType type)
    {
        float out = 0.0f;

        switch (type)
        {
            case OscillatorType::Sine:
                out = std::sin (TWO_PI * phase);
                break;

            case OscillatorType::Saw:
                out = 2.0f * phase - 1.0f;
                out -= polyBLEP (phase, increment);
                break;

            case OscillatorType::Square:
                out = nextPWM (0.5f);
                break;

            case OscillatorType::Triangle:
                out = nextTriAA();
                break;

            case OscillatorType::Noise:
                out = noiseDist (rng);
                break;
        }

        advancePhase();
        return out;
    }

    // === PWM: Square con pulse width variable ===
    // pw: 0.01 .. 0.99 (0.5 = square clasico)
    float nextPWM (float pw)
    {
        pw = std::max (0.01f, std::min (pw, 0.99f));

        // Dos saws desfasados → PWM con anti-aliasing PolyBLEP
        float saw1 = 2.0f * phase - 1.0f;
        saw1 -= polyBLEP (phase, increment);

        float phase2 = phase + pw;
        if (phase2 >= 1.0f) phase2 -= 1.0f;

        float saw2 = 2.0f * phase2 - 1.0f;
        saw2 -= polyBLEP (phase2, increment);

        return (saw1 - saw2) * 0.5f;
    }

    // === Wave Morph: crossfade continuo saw ↔ square ===
    // morph: 0.0 = saw puro, 1.0 = square puro
    float nextMorph (float morph)
    {
        float saw = 2.0f * phase - 1.0f;
        saw -= polyBLEP (phase, increment);

        float sq = nextPWM (0.5f);

        return saw * (1.0f - morph) + sq * morph;
    }

    // === Sub-oscillator: sine a octava inferior ===
    float nextSub()
    {
        float out = std::sin (TWO_PI * subPhase);
        subPhase += increment * 0.5f;
        if (subPhase >= 1.0f) subPhase -= 1.0f;
        return out;
    }

    // Genera un seno a una frecuencia arbitraria sin cambiar el estado del oscilador
    float sineAt (float freq, float sampleRate, float time) const
    {
        return std::sin (TWO_PI * freq * time);
    }

    float getPhase() const { return phase; }
    float getIncrement() const { return increment; }

    // Avanzar fase manualmente (llamar solo si NO usas next())
    void advancePhase()
    {
        phase += increment;
        if (phase >= 1.0f) phase -= 1.0f;
    }

private:
    float phase = 0.0f;
    float subPhase = 0.0f;
    float increment = 0.0f;

    // Triangle anti-aliased state
    float lastSawForTri = 0.0f;
    float triIntegrator = 0.0f;

    std::mt19937 rng { 42 };
    std::uniform_real_distribution<float> noiseDist { -1.0f, 1.0f };

    // Triangle anti-aliased via leaky integrator del saw PolyBLEP
    float nextTriAA()
    {
        // Generar saw con PolyBLEP
        float saw = 2.0f * phase - 1.0f;
        saw -= polyBLEP (phase, increment);

        // Integrar el saw para obtener triangle (anti-aliased)
        // Factor de leakiness basado en la frecuencia para estabilidad
        float leaky = 1.0f - increment * 2.0f;
        leaky = std::max (0.98f, std::min (leaky, 0.9999f));

        triIntegrator = leaky * triIntegrator + saw * increment * 4.0f;

        // Normalizar: el integrador produce ~1/pi de amplitud
        return std::max (-1.0f, std::min (triIntegrator * PI, 1.0f));
    }

    static float polyBLEP (float t, float dt)
    {
        if (dt <= 0.0f) return 0.0f;

        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    static float wrap (float p)
    {
        if (p >= 1.0f) p -= 1.0f;
        return p;
    }
};

} // namespace dsputil

#pragma once

#include <cmath>
#include <algorithm>

namespace dsputil
{

// Envelope exponencial simple: sube linealmente y decae exponencialmente
class ExpDecay
{
public:
    void trigger (float decayTimeSec, float sr)
    {
        // -60 dB en decayTime
        coefficient = std::exp (-6.9f / std::max (0.001f, decayTimeSec * sr));
        level = 1.0f;
        active = true;
    }

    float next()
    {
        if (! active) return 0.0f;
        float out = level;
        level *= coefficient;
        if (level < 0.0001f) { level = 0.0f; active = false; }
        return out;
    }

    bool isActive() const { return active; }
    float getLevel() const { return level; }

private:
    float coefficient = 0.0f;
    float level = 0.0f;
    bool active = false;
};

// Envelope ADSR completo con ataque curvado para calidad profesional
// attackCurve: 0 = lineal, 1 = exponencial completo (concavo, suave)
class ADSREnvelope
{
public:
    void setParameters (float a, float d, float s, float r, float sr)
    {
        attackTime   = std::max (1.0f, a * sr);
        attackRate   = 1.0f / attackTime;
        decayCoeff   = std::exp (-6.9f / std::max (1.0f, d * sr));
        sustainLevel = s;
        releaseCoeff = std::exp (-6.9f / std::max (1.0f, r * sr));
        sampleRate   = sr;
    }

    // attackCurve: 0..1 (0 = lineal, 1 = exponencial suave)
    void setAttackCurve (float curve) { attackCurve = curve; }

    void trigger()
    {
        stage = Stage::Attack;
        level = 0.0f;
        attackSampleCount = 0.0f;
    }

    void startRelease()
    {
        if (stage != Stage::Off)
            stage = Stage::Release;
    }

    float next()
    {
        switch (stage)
        {
            case Stage::Attack:
            {
                attackSampleCount += 1.0f;
                float linear = attackSampleCount / attackTime;

                if (linear >= 1.0f)
                {
                    level = 1.0f;
                    stage = Stage::Decay;
                }
                else
                {
                    // Curva exponencial concava: empieza rapido, termina suave
                    // Mezcla entre lineal y exponencial segun attackCurve
                    float expo = 1.0f - std::exp (-linear * 5.0f);  // -5 = curva pronunciada
                    expo /= (1.0f - std::exp (-5.0f));  // normalizar a [0,1]
                    level = linear * (1.0f - attackCurve) + expo * attackCurve;
                }
                break;
            }

            case Stage::Decay:
                level = sustainLevel + (level - sustainLevel) * decayCoeff;
                if (level <= sustainLevel + 0.001f)
                {
                    level = sustainLevel;
                    stage = (sustainLevel > 0.001f) ? Stage::Sustain : Stage::Off;
                }
                break;

            case Stage::Sustain:
                level = sustainLevel;
                break;

            case Stage::Release:
                level *= releaseCoeff;
                if (level < 0.0001f) { level = 0.0f; stage = Stage::Off; }
                break;

            case Stage::Off:
                level = 0.0f;
                break;
        }
        return level;
    }

    bool isActive() const { return stage != Stage::Off; }
    float getLevel() const { return level; }

private:
    enum class Stage { Attack, Decay, Sustain, Release, Off };
    Stage stage = Stage::Off;
    float level = 0.0f;
    float attackTime = 100.0f;
    float attackRate = 0.01f;
    float attackSampleCount = 0.0f;
    float attackCurve = 0.7f;  // Default: mostly exponential (suave)
    float decayCoeff = 0.9999f;
    float sustainLevel = 0.0f;
    float releaseCoeff = 0.9999f;
    float sampleRate = 44100.0f;
};

// Envelope curvado simple para one-shots (attack + decay sin sustain)
// Ideal para pads y texturas donde quieres control preciso de la forma
class CurvedEnvelope
{
public:
    void trigger (float attackSec, float decaySec, float peakLevel, float sr,
                  float curve = 0.7f)
    {
        attackSamples = std::max (1.0f, attackSec * sr);
        decayCoeff = std::exp (-6.9f / std::max (1.0f, decaySec * sr));
        peak = peakLevel;
        shapeCurve = curve;
        sampleCount = 0.0f;
        level = 0.0f;
        inAttack = true;
        active = true;
    }

    float next()
    {
        if (! active) return 0.0f;

        if (inAttack)
        {
            sampleCount += 1.0f;
            float t = sampleCount / attackSamples;

            if (t >= 1.0f)
            {
                level = peak;
                inAttack = false;
            }
            else
            {
                // Smooth S-curve attack
                float linear = t;
                float expo = 1.0f - std::exp (-t * 5.0f);
                expo /= (1.0f - std::exp (-5.0f));
                level = peak * (linear * (1.0f - shapeCurve) + expo * shapeCurve);
            }
        }
        else
        {
            level *= decayCoeff;
            if (level < 0.0001f) { level = 0.0f; active = false; }
        }

        return level;
    }

    bool isActive() const { return active; }

private:
    float attackSamples = 100.0f;
    float decayCoeff = 0.999f;
    float peak = 1.0f;
    float shapeCurve = 0.7f;
    float sampleCount = 0.0f;
    float level = 0.0f;
    bool inAttack = true;
    bool active = false;
};

} // namespace dsputil

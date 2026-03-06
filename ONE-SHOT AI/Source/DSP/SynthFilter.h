#pragma once

#include <cmath>
#include <algorithm>
#include "DSPConstants.h"
#include "../Params/SynthEnums.h"

namespace dsputil
{

// Filtro SVF (State Variable Filter) con Topology Preserving Transform
// Basado en el diseno de Andrew Simper (Cytomic)
class SVFilter
{
public:
    void setParameters (float cutoffHz, float resonance, FilterType filterType, float sampleRate)
    {
        type = filterType;
        cutoffHz  = std::max (20.0f, std::min (cutoffHz, sampleRate * 0.49f));
        resonance = std::max (0.0f, std::min (resonance, 1.0f));

        float g = std::tan (PI * cutoffHz / sampleRate);
        k  = 2.0f - 2.0f * resonance * 0.98f;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    float process (float x)
    {
        float v3 = x - ic2eq;
        float v1 = a1 * ic1eq + a2 * v3;
        float v2 = ic2eq + a2 * v1 + a3 * v3;
        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;

        switch (type)
        {
            case FilterType::LowPass:  return v2;
            case FilterType::BandPass: return v1;
            case FilterType::HighPass: return x - k * v1 - v2;
        }
        return v2;
    }

    void reset()
    {
        ic1eq = 0.0f;
        ic2eq = 0.0f;
    }

private:
    float ic1eq = 0.0f, ic2eq = 0.0f;
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.0f;
    FilterType type = FilterType::LowPass;
};

// Filtro SVF 4-pole (24dB/oct) — cascada de dos SVF
// Mucho mas gordo y resonante que el 2-pole. Ideal para bass, pads, leads.
class SVFilter4Pole
{
public:
    void setParameters (float cutoffHz, float resonance, FilterType filterType, float sampleRate)
    {
        // La resonancia se distribuye entre ambas etapas
        float q1 = resonance * 0.7f;
        float q2 = resonance * 0.5f;
        stage1.setParameters (cutoffHz, q1, filterType, sampleRate);
        stage2.setParameters (cutoffHz, q2, filterType, sampleRate);
    }

    float process (float x)
    {
        return stage2.process (stage1.process (x));
    }

    void reset()
    {
        stage1.reset();
        stage2.reset();
    }

private:
    SVFilter stage1, stage2;
};

// Filtro one-pole simple para suavizado y coloracion de ruido
class OnePole
{
public:
    void setLowPass (float cutoffHz, float sampleRate)
    {
        float x = std::exp (-TWO_PI * cutoffHz / sampleRate);
        a = 1.0f - x;
        b = x;
        isHP = false;
    }

    void setHighPass (float cutoffHz, float sampleRate)
    {
        float x = std::exp (-TWO_PI * cutoffHz / sampleRate);
        a = x;
        b = x;
        isHP = true;
    }

    float process (float x)
    {
        if (isHP)
        {
            float out = a * (x - prevInput) + b * z;
            prevInput = x;
            z = out;
            return out;
        }
        z = a * x + b * z;
        return z;
    }

    void reset()
    {
        z = 0.0f;
        prevInput = 0.0f;
    }

private:
    float a = 1.0f, b = 0.0f, z = 0.0f;
    float prevInput = 0.0f;
    bool isHP = false;
};

// Smooth parameter follower (one-pole smoother)
// Util para evitar zipper noise en cambios de parametros
class ParamSmoother
{
public:
    void setSpeed (float smoothTimeMs, float sampleRate)
    {
        coeff = std::exp (-TWO_PI / (smoothTimeMs * 0.001f * sampleRate));
    }

    float process (float target)
    {
        current = target + coeff * (current - target);
        return current;
    }

    void reset (float value = 0.0f) { current = value; }

private:
    float coeff = 0.999f;
    float current = 0.0f;
};

} // namespace dsputil

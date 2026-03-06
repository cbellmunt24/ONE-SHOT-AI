#pragma once

#include <cmath>
#include <algorithm>

namespace dsputil
{

enum class SaturationMode
{
    Tape,       // asinh suave — warmth sin harshness (ideal para pads, master)
    Tube,       // asimetrica — even harmonics = calidez analoga (ideal para bass, leads)
    SoftClip,   // tanh clasica — neutral, transparente
    HardClip,   // clip directo — agresivo, digital
    Fold        // wavefolder — metalico, sintesis (ideal para leads agresivos)
};

class Saturator
{
public:
    // Procesa con modo especifico. drive: 0..1 (0=limpio, 1=saturacion fuerte)
    float process (float x, float drive, SaturationMode mode = SaturationMode::SoftClip)
    {
        if (drive <= 0.0f) return x;

        float wet = 0.0f;

        switch (mode)
        {
            case SaturationMode::Tape:
            {
                // asinh: compresion suave, preserva transientes, warmth
                float gain = 1.0f + drive * 4.0f;
                float driven = x * gain;
                // asinh normalizado
                float asinhVal = std::log (driven + std::sqrt (driven * driven + 1.0f));
                float asinhNorm = std::log (gain + std::sqrt (gain * gain + 1.0f));
                wet = (asinhNorm > 0.001f) ? asinhVal / asinhNorm : driven;
                break;
            }

            case SaturationMode::Tube:
            {
                // Asimetrica: positivos se saturan mas que negativos → even harmonics
                float gain = 1.0f + drive * 6.0f;
                float driven = x * gain;
                if (driven >= 0.0f)
                    wet = std::tanh (driven * 1.2f);
                else
                    wet = std::tanh (driven * 0.8f);
                // Normalizar
                float normFactor = std::tanh (gain);
                if (normFactor > 0.001f) wet /= normFactor;
                break;
            }

            case SaturationMode::SoftClip:
            {
                // tanh clasica
                float gain = 1.0f + drive * 10.0f;
                float tanhGain = std::tanh (gain);
                wet = (tanhGain > 0.001f) ? std::tanh (x * gain) / tanhGain : x;
                break;
            }

            case SaturationMode::HardClip:
            {
                float gain = 1.0f + drive * 8.0f;
                wet = std::max (-1.0f, std::min (x * gain, 1.0f));
                break;
            }

            case SaturationMode::Fold:
            {
                // Wavefolder: senial se "pliega" al exceder [-1,1]
                float gain = 1.0f + drive * 6.0f;
                float driven = x * gain;
                // Triangle fold
                wet = std::asin (std::sin (driven * 1.5707963f)) * 0.6366198f;
                break;
            }
        }

        // Makeup gain: compensa la pérdida de nivel por saturación
        float makeup = 1.0f + drive * 0.3f;
        wet *= makeup;

        // Dry/wet segun drive
        float blend = std::min (drive * 2.0f, 1.0f);
        return x * (1.0f - blend) + wet * blend;
    }

    // Backward compat: tanh simple
    float process (float x, float drive)
    {
        return process (x, drive, SaturationMode::SoftClip);
    }
};

} // namespace dsputil

#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <algorithm>
#include "../DSP/DSPConstants.h"
#include "../DSP/DCBlocker.h"

namespace synthutil
{

// MIDI note a frecuencia (69 = A4 = 440 Hz)
inline float midiToFreq (float midiNote)
{
    return 440.0f * std::pow (2.0f, (midiNote - 69.0f) / 12.0f);
}

// Pan mono a estereo (pan: 0=L, 0.5=C, 1=R)
inline void panMonoToStereo (float sample, float pan, float& left, float& right)
{
    left  = sample * std::cos (pan * dsputil::PI * 0.5f);
    right = sample * std::sin (pan * dsputil::PI * 0.5f);
}

// Aplica fade-out con curva S (cosine) para transicion mas natural
inline void applyFadeOut (juce::AudioBuffer<float>& buffer, int fadeSamples)
{
    int total = buffer.getNumSamples();
    fadeSamples = std::min (fadeSamples, total);
    int start = total - fadeSamples;

    for (int i = start; i < total; ++i)
    {
        float phase = (float) (i - start) / (float) fadeSamples; // 0..1
        // S-curve: cosine fade (suave al inicio y final, no lineal)
        float gain = 0.5f * (1.0f + std::cos (dsputil::PI * phase));
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample (ch, i, buffer.getSample (ch, i) * gain);
    }
}

// Calcula duracion total en samples a partir de segundos
inline int durationInSamples (float seconds, double sampleRate)
{
    return (int) (seconds * sampleRate);
}

// Limita un valor entre min y max
inline float clamp (float value, float minVal, float maxVal)
{
    return std::max (minVal, std::min (value, maxVal));
}

// DC blocker para buffer completo (elimina offset DC post-sintesis)
inline void dcBlockBuffer (juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        dsputil::DCBlocker dc;
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            data[i] = dc.process (data[i]);
    }
}

// Soft clipping con soft-knee real — preserva transientes
// threshold alto (0.85) + drive suave → solo satura picos extremos
inline void softClipBuffer (juce::AudioBuffer<float>& buffer, float drive = 1.05f)
{
    if (drive < 1.001f) return;

    const float threshold = 0.85f;
    float invTanh = 1.0f / std::tanh (drive);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float x = data[i];
            float absX = std::abs (x);
            if (absX < threshold)
            {
                // Zona lineal — sin distorsion, preserva transientes
            }
            else
            {
                // Zona de saturacion con tanh suave
                float sign = (x >= 0.0f) ? 1.0f : -1.0f;
                float excess = (absX - threshold) * drive;
                float saturated = threshold + (1.0f - threshold) * std::tanh (excess) * invTanh;
                data[i] = sign * saturated;
            }
        }
    }
}

// Brick-wall limiter simple — instant clip al ceiling, sin lookahead
// Para one-shots no necesitamos lookahead (sin pre-ecos ni pumping)
inline void truePeakLimiter (juce::AudioBuffer<float>& buffer, float /*sampleRate*/,
                             float ceilingDb = -0.3f)
{
    float ceiling = std::pow (10.0f, ceilingDb / 20.0f);
    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float s = buffer.getSample (ch, i);
            if (s > ceiling)       s = ceiling;
            else if (s < -ceiling) s = -ceiling;
            buffer.setSample (ch, i, s);
        }
    }
}

// Normaliza el buffer a un nivel de pico objetivo (por defecto -0.3 dBFS)
inline void normalizeBuffer (juce::AudioBuffer<float>& buffer, float targetPeak = 0.95f)
{
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* data = buffer.getReadPointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float v = std::abs (data[i]);
            if (v > peak) peak = v;
        }
    }
    if (peak > 0.0001f)
        buffer.applyGain (targetPeak / peak);
}

} // namespace synthutil

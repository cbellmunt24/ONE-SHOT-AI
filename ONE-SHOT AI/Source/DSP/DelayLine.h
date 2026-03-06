#pragma once

#include <vector>
#include <algorithm>
#include <cmath>

namespace dsputil
{

class DelayLine
{
public:
    void setSize (int maxSamples)
    {
        buffer.resize ((size_t) maxSamples, 0.0f);
        writePos = 0;
    }

    void write (float sample)
    {
        if (buffer.empty()) return;
        buffer[(size_t) writePos] = sample;
        writePos = (writePos + 1) % (int) buffer.size();
    }

    // Lectura con retardo entero
    float read (int delaySamples) const
    {
        if (buffer.empty()) return 0.0f;
        int readPos = writePos - delaySamples;
        while (readPos < 0) readPos += (int) buffer.size();
        return buffer[(size_t) readPos];
    }

    // Lectura con retardo fraccionario (interpolacion lineal) - backward compat
    float readInterpolated (float delaySamples) const
    {
        if (buffer.empty()) return 0.0f;
        float readPos = (float) writePos - delaySamples;
        while (readPos < 0.0f) readPos += (float) buffer.size();

        int pos0 = (int) readPos % (int) buffer.size();
        int pos1 = (pos0 + 1) % (int) buffer.size();
        float frac = readPos - std::floor (readPos);

        return buffer[(size_t) pos0] * (1.0f - frac) + buffer[(size_t) pos1] * frac;
    }

    // Interpolacion cubica Hermite - mucho mas precisa para Karplus-Strong y chorus
    // Reduce artefactos de alta frecuencia vs lineal
    float readHermite (float delaySamples) const
    {
        if (buffer.empty()) return 0.0f;

        float readPos = (float) writePos - delaySamples;
        while (readPos < 0.0f) readPos += (float) buffer.size();

        int sz = (int) buffer.size();
        int pos1 = (int) readPos % sz;
        int pos0 = (pos1 - 1 + sz) % sz;
        int pos2 = (pos1 + 1) % sz;
        int pos3 = (pos1 + 2) % sz;

        float frac = readPos - std::floor (readPos);

        float y0 = buffer[(size_t) pos0];
        float y1 = buffer[(size_t) pos1];
        float y2 = buffer[(size_t) pos2];
        float y3 = buffer[(size_t) pos3];

        // Hermite interpolation
        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    // Allpass interpolation - ideal para Karplus-Strong (preserva todas las frecuencias)
    float readAllpass (float delaySamples)
    {
        if (buffer.empty()) return 0.0f;

        int intDelay = (int) delaySamples;
        float frac = delaySamples - (float) intDelay;

        // Allpass coefficient
        float coeff = (1.0f - frac) / (1.0f + frac);

        float readSample = read (intDelay);
        float out = coeff * (readSample - allpassState) + readSample;

        // Usar el sample de intDelay-1 para el estado
        allpassState = out;

        return out;
    }

    void clear()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        allpassState = 0.0f;
    }

private:
    std::vector<float> buffer;
    int writePos = 0;
    float allpassState = 0.0f;
};

} // namespace dsputil

#pragma once

namespace dsputil
{

// DC Blocker: high-pass filter a ~5Hz para eliminar offset DC
// Reutilizable en cualquier synth (reemplaza el inline DC block del KickSynth)
class DCBlocker
{
public:
    void reset()
    {
        x1 = 0.0f;
        y1 = 0.0f;
    }

    float process (float x)
    {
        float y = x - x1 + coeff * y1;
        x1 = x;
        y1 = y;
        return y;
    }

private:
    float x1 = 0.0f;
    float y1 = 0.0f;
    static constexpr float coeff = 0.9986f; // ~5Hz @ 44.1kHz
};

} // namespace dsputil

#pragma once

#include <JuceHeader.h>
#include "../Params/SynthParams.h"
#include "../Effects/EffectChain.h"
#include "KickSynth.h"
#include "SnareSynth.h"
#include "HiHatSynth.h"
#include "ClapSynth.h"
#include "PercSynth.h"
#include "Bass808Synth.h"
#include "LeadSynth.h"
#include "PluckSynth.h"
#include "PadSynth.h"
#include "TextureSynth.h"

// Motor de sintesis principal — UPGRADED master chain
//
// Orden: generate → effects → DC block → soft clip → limiter → normalize
// DC blocking elimina offset post-sintesis
// True peak limiter previene inter-sample peaks
// Normalizacion a -0.3 dBFS para loudness profesional

class SynthEngine
{
public:
    // Genera un one-shot completo a partir de un GenerationResult.
    // Devuelve un AudioBuffer estereo con el sonido renderizado.
    juce::AudioBuffer<float> generate (const GenerationResult& result,
                                       double sampleRate,
                                       unsigned int seed = 0)
    {
        // Si seed es 0, generar uno aleatorio
        if (seed == 0)
        {
            std::random_device rd;
            seed = rd();
        }

        auto buffer = std::visit ([&] (const auto& params) -> juce::AudioBuffer<float>
        {
            return generateForInstrument (params, sampleRate, seed);
        }, result.params);

        // === FASE 4: CADENA DE EFECTOS DSP ===
        effectChain.process (buffer, result.effects, (float) sampleRate);

        // === MASTER POST-PROCESSING (UPGRADED) ===

        // 1. DC blocker: elimina offset DC post-sintesis/efectos
        synthutil::dcBlockBuffer (buffer);

        // 2. Soft clip suave para warmth y control de picos
        synthutil::softClipBuffer (buffer, 1.08f);

        // 3. Brick-wall limiter: previene picos sobre ceiling (-0.3 dBFS)
        synthutil::truePeakLimiter (buffer, (float) sampleRate, -0.3f);

        // 4. Normalizar a ~-0.8 dBFS para loudness profesional
        synthutil::normalizeBuffer (buffer, 0.91f);

        return buffer;
    }

private:
    // Dispatch por tipo de parametros
    juce::AudioBuffer<float> generateForInstrument (const KickParams& p, double sr, unsigned int seed)
    {
        return kickSynth.generate (p, sr, seed);
    }

    juce::AudioBuffer<float> generateForInstrument (const SnareParams& p, double sr, unsigned int seed)
    {
        return snareSynth.generate (p, sr, seed);
    }

    juce::AudioBuffer<float> generateForInstrument (const HiHatParams& p, double sr, unsigned int seed)
    {
        return hiHatSynth.generate (p, sr, seed);
    }

    juce::AudioBuffer<float> generateForInstrument (const ClapParams& p, double sr, unsigned int seed)
    {
        return clapSynth.generate (p, sr, seed);
    }

    juce::AudioBuffer<float> generateForInstrument (const PercParams& p, double sr, unsigned int seed)
    {
        return percSynth.generate (p, sr, seed);
    }

    juce::AudioBuffer<float> generateForInstrument (const Bass808Params& p, double sr, unsigned int seed)
    {
        return bass808Synth.generate (p, sr, seed);
    }

    juce::AudioBuffer<float> generateForInstrument (const LeadParams& p, double sr, unsigned int seed)
    {
        return leadSynth.generate (p, sr, seed);
    }

    juce::AudioBuffer<float> generateForInstrument (const PluckParams& p, double sr, unsigned int seed)
    {
        return pluckSynth.generate (p, sr, seed);
    }

    juce::AudioBuffer<float> generateForInstrument (const PadParams& p, double sr, unsigned int seed)
    {
        return padSynth.generate (p, sr, seed);
    }

    juce::AudioBuffer<float> generateForInstrument (const TextureParams& p, double sr, unsigned int seed)
    {
        return textureSynth.generate (p, sr, seed);
    }

    // Instancias de sintetizadores
    KickSynth    kickSynth;
    SnareSynth   snareSynth;
    HiHatSynth   hiHatSynth;
    ClapSynth    clapSynth;
    PercSynth    percSynth;
    Bass808Synth bass808Synth;
    LeadSynth    leadSynth;
    PluckSynth   pluckSynth;
    PadSynth     padSynth;
    TextureSynth textureSynth;

    // Cadena de efectos DSP (Fase 4)
    EffectChain  effectChain;
};

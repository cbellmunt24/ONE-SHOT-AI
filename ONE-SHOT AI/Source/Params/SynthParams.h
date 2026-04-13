#pragma once

// Master include para todo el espacio paramétrico
#include "SynthEnums.h"
#include "BaseSoundParams.h"
#include "InstrumentParams.h"
#include "GenerationRequest.h"
#include "../Effects/EffectParams.h"
#include "../UniversalSynth/UniversalSynthParams.h"

#include <variant>

// Legacy variant type — kept for backward compatibility with old code paths.
// New code should use UniversalSynthParams directly.
using InstrumentParamsVariant = std::variant<
    KickParams,
    SnareParams,
    HiHatParams,
    ClapParams,
    PercParams,
    Bass808Params,
    LeadParams,
    PluckParams,
    PadParams,
    TextureParams
>;

// Resultado completo de la generación: contiene los parámetros universales
// del sintetizador listos para ser procesados por el motor de síntesis.
struct GenerationResult
{
    InstrumentType                      instrument;
    universalsynth::UniversalSynthParams universalParams;  // New: universal synth params
    InstrumentParamsVariant             legacyParams;      // Legacy: kept for backward compat
    EffectChainParams                   effects;
};

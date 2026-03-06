#pragma once

// Master include para todo el espacio paramétrico
#include "SynthEnums.h"
#include "BaseSoundParams.h"
#include "InstrumentParams.h"
#include "GenerationRequest.h"
#include "../Effects/EffectParams.h"

#include <variant>

// Variante que contiene los parámetros específicos de cualquier instrumento.
// Permite pasar un único tipo que represente el resultado de la generación
// sin importar qué instrumento se haya seleccionado.
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

// Resultado completo de la generación: contiene los parámetros específicos
// del instrumento listos para ser procesados por el motor de síntesis.
struct GenerationResult
{
    InstrumentType          instrument;
    InstrumentParamsVariant params;
    EffectChainParams       effects;   // Fase 4: cadena de efectos DSP
};

#pragma once

#include "SynthEnums.h"

// Controles de carácter del sonido (sliders 0..1 del UX)
struct CharacterControls
{
    float brillo        = 0.5f;     // 0..1, presencia de agudos / claridad
    float cuerpo        = 0.5f;     // 0..1, sensación de plenitud / grosor
    float textura       = 0.5f;     // 0..1, suave a granular/áspero
    float movimiento    = 0.0f;     // 0..1, vibrato, drift, modulación interna
};

// Petición completa de generación de un one-shot
struct GenerationRequest
{
    InstrumentType  instrument  = InstrumentType::Kick;
    GenreStyle      genre       = GenreStyle::Trap;
    AttackType      attack      = AttackType::Fast;
    EnergyLevel     energy      = EnergyLevel::Medium;

    CharacterControls character;

    float impacto   = 0.5f;     // 0..1, pegada y presencia percibida

    // Seed para reproducibilidad (0 = aleatorio)
    unsigned int seed = 0;
};

#pragma once

// Tipos de instrumento disponibles para generación de one-shots
enum class InstrumentType
{
    Kick,
    Snare,
    HiHat,
    Clap,
    Perc,
    Bass808,
    Lead,
    Pluck,
    Pad,
    Texture
};

// Estilos/géneros musicales que condicionan la generación
enum class GenreStyle
{
    Trap,
    HipHop,
    Techno,
    House,
    Reggaeton,
    Afrobeat,
    RnB,
    EDM,
    Ambient
};

// Velocidad del ataque del sonido
enum class AttackType
{
    Fast,
    Medium,
    Slow
};

// Nivel de energía general del sonido
enum class EnergyLevel
{
    Low,
    Medium,
    High
};

// Formas de onda del oscilador principal
enum class OscillatorType
{
    Sine,
    Saw,
    Square,
    Triangle,
    Noise
};

// Tipos de filtro
enum class FilterType
{
    LowPass,
    HighPass,
    BandPass
};

// Destino de modulación del LFO
enum class LfoDestination
{
    None,
    Pitch,
    FilterCutoff,
    Amplitude,
    Pan
};

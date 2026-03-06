#pragma once

// Parámetros para todos los efectos DSP de la Fase 4.
// Cada efecto tiene un flag 'enabled' y un 'mix' (dry/wet).
// EffectChainParams agrupa todos los efectos en una cadena ordenada.

// ============================================================
// REVERB
// ============================================================
struct ReverbParams
{
    bool  enabled   = false;
    float size      = 0.5f;   // 0..1 — tamaño de sala
    float decay     = 0.5f;   // 0..1 — tiempo de decaimiento
    float damping   = 0.5f;   // 0..1 — absorción de agudos
    float predelay  = 0.01f;  // segundos — pre-delay
    float mix       = 0.2f;   // 0..1 — dry/wet
};

// ============================================================
// DELAY
// ============================================================
struct DelayParams
{
    bool  enabled   = false;
    float time      = 0.25f;  // segundos — tiempo de delay
    float feedback  = 0.3f;   // 0..1 — retroalimentación
    float highCut   = 8000.0f; // Hz — filtro pasa-bajos en feedback
    float mix       = 0.2f;   // 0..1 — dry/wet
    bool  stereo    = true;   // ping-pong stereo
};

// ============================================================
// CHORUS
// ============================================================
struct ChorusParams
{
    bool  enabled   = false;
    float rate      = 1.5f;   // Hz — velocidad LFO
    float depth     = 0.5f;   // 0..1 — profundidad de modulación
    float mix       = 0.3f;   // 0..1 — dry/wet
};

// ============================================================
// PHASER
// ============================================================
struct PhaserParams
{
    bool  enabled   = false;
    float rate      = 0.5f;   // Hz — velocidad LFO
    float depth     = 0.5f;   // 0..1 — profundidad
    float feedback  = 0.3f;   // 0..1 — retroalimentación
    int   stages    = 4;      // 2,4,6,8 — número de etapas allpass
    float mix       = 0.3f;   // 0..1 — dry/wet
};

// ============================================================
// EQ (3 bandas)
// ============================================================
struct EQParams
{
    bool  enabled   = false;
    float lowGain   = 0.0f;   // dB — ganancia de graves (-12..+12)
    float lowFreq   = 200.0f; // Hz — frecuencia de corte de graves
    float midGain   = 0.0f;   // dB — ganancia de medios (-12..+12)
    float midFreq   = 1000.0f; // Hz — frecuencia central de medios
    float midQ      = 1.0f;   // Q — ancho de banda de medios
    float highGain  = 0.0f;   // dB — ganancia de agudos (-12..+12)
    float highFreq  = 5000.0f; // Hz — frecuencia de corte de agudos
};

// ============================================================
// DISTORTION / SATURATION
// ============================================================
struct DistortionParams
{
    bool  enabled   = false;
    float drive     = 0.3f;   // 0..1 — cantidad de distorsión
    float tone      = 0.5f;   // 0..1 — tono post-distorsión (LP filter)
    float mix       = 0.5f;   // 0..1 — dry/wet
};

// ============================================================
// COMPRESSOR
// ============================================================
struct CompressorParams
{
    bool  enabled     = false;
    float threshold   = -12.0f; // dB — umbral
    float ratio       = 4.0f;   // ratio de compresión (1..20)
    float attackMs    = 5.0f;   // ms — ataque
    float releaseMs   = 50.0f;  // ms — release
    float makeupGain  = 0.0f;   // dB — ganancia de compensación
    float mix         = 1.0f;   // 0..1 — dry/wet (parallel compression)
};

// ============================================================
// BITCRUSHER
// ============================================================
struct BitcrusherParams
{
    bool  enabled     = false;
    float bitDepth    = 16.0f;  // bits (1..16)
    float sampleRateReduction = 1.0f; // factor de reducción (1=ninguna, 32=máxima)
    float mix         = 0.5f;   // 0..1 — dry/wet
};

// ============================================================
// RING MODULATOR
// ============================================================
struct RingModParams
{
    bool  enabled     = false;
    float frequency   = 440.0f; // Hz — frecuencia del oscilador
    float mix         = 0.3f;   // 0..1 — dry/wet
};

// ============================================================
// EFFECT CHAIN — agrupa todos los efectos
// ============================================================
struct EffectChainParams
{
    ReverbParams      reverb;
    DelayParams       delay;
    ChorusParams      chorus;
    PhaserParams      phaser;
    EQParams          eq;
    DistortionParams  distortion;
    CompressorParams  compressor;
    BitcrusherParams  bitcrusher;
    RingModParams     ringMod;
};

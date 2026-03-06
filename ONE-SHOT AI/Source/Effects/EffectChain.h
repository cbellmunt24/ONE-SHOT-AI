#pragma once

#include <JuceHeader.h>
#include "EffectParams.h"
#include "EQEffect.h"
#include "CompressorEffect.h"
#include "DistortionEffect.h"
#include "ChorusEffect.h"
#include "PhaserEffect.h"
#include "ReverbEffect.h"
#include "DelayEffect.h"
#include "BitcrusherEffect.h"
#include "RingModEffect.h"

// Cadena de efectos principal.
// Aplica todos los efectos habilitados en orden óptimo:
//   1. EQ (corrección tonal pre-efectos)
//   2. Compressor (controlar dinámica antes de efectos)
//   3. Distortion (waveshaping sobre señal comprimida)
//   4. Bitcrusher (reducción lo-fi)
//   5. Ring Mod (modulación)
//   6. Chorus (ensanchamiento estéreo)
//   7. Phaser (movimiento espectral)
//   8. Delay (repeticiones temporales)
//   9. Reverb (espacio — siempre al final)

class EffectChain
{
public:
    void process (juce::AudioBuffer<float>& buffer, const EffectChainParams& params, float sampleRate)
    {
        eq.process (buffer, params.eq, sampleRate);
        compressor.process (buffer, params.compressor, sampleRate);
        distortion.process (buffer, params.distortion, sampleRate);
        bitcrusher.process (buffer, params.bitcrusher, sampleRate);
        ringMod.process (buffer, params.ringMod, sampleRate);
        chorus.process (buffer, params.chorus, sampleRate);
        phaser.process (buffer, params.phaser, sampleRate);
        delay.process (buffer, params.delay, sampleRate);
        reverb.process (buffer, params.reverb, sampleRate);
    }

private:
    EQEffect          eq;
    CompressorEffect  compressor;
    DistortionEffect  distortion;
    BitcrusherEffect  bitcrusher;
    RingModEffect     ringMod;
    ChorusEffect      chorus;
    PhaserEffect      phaser;
    DelayEffect       delay;
    ReverbEffect      reverb;
};

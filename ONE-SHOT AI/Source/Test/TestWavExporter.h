#pragma once

#include <JuceHeader.h>
#include "../AI/ParameterGenerator.h"
#include "../SynthEngine/SynthEngine.h"

// Exportador de test: genera .wav para cada combinación instrumento × género.
// Permite escuchar y validar la calidad de síntesis antes de integrar la UX.

class TestWavExporter
{
public:
    static int exportAll (const juce::File& outputDir)
    {
        outputDir.createDirectory();

        ParameterGenerator paramGen;
        SynthEngine engine;

        const double sampleRate = 44100.0;
        const unsigned int baseSeed = 12345;

        const char* instrumentNames[] = {
            "Kick", "Snare", "HiHat", "Clap", "Perc",
            "Bass808", "Lead", "Pluck", "Pad", "Texture"
        };

        const char* genreNames[] = {
            "Trap", "HipHop", "Techno", "House", "Reggaeton",
            "Afrobeat", "RnB", "EDM", "Ambient"
        };

        const InstrumentType instruments[] = {
            InstrumentType::Kick,    InstrumentType::Snare,  InstrumentType::HiHat,
            InstrumentType::Clap,    InstrumentType::Perc,   InstrumentType::Bass808,
            InstrumentType::Lead,    InstrumentType::Pluck,  InstrumentType::Pad,
            InstrumentType::Texture
        };

        const GenreStyle genres[] = {
            GenreStyle::Trap,     GenreStyle::HipHop,   GenreStyle::Techno,
            GenreStyle::House,    GenreStyle::Reggaeton, GenreStyle::Afrobeat,
            GenreStyle::RnB,      GenreStyle::EDM,       GenreStyle::Ambient
        };

        int totalFiles = 0;

        for (int i = 0; i < 10; ++i)
        {
            // Crear subcarpeta por instrumento
            juce::File instrDir = outputDir.getChildFile (instrumentNames[i]);
            instrDir.createDirectory();

            for (int g = 0; g < 9; ++g)
            {
                GenerationRequest req;
                req.instrument = instruments[i];
                req.genre      = genres[g];
                req.attack     = AttackType::Fast;
                req.energy     = EnergyLevel::High;
                req.character  = { 0.5f, 0.5f, 0.5f, 0.3f };
                req.impacto    = 0.8f;
                req.seed       = baseSeed + (unsigned int) totalFiles;

                auto result = paramGen.generate (req, req.seed);
                auto buffer = engine.generate (result, sampleRate, req.seed);

                juce::String filename = juce::String (instrumentNames[i]) + "_"
                                      + juce::String (genreNames[g]) + ".wav";

                juce::File outFile = instrDir.getChildFile (filename);
                writeWav (outFile, buffer, sampleRate);
                totalFiles++;
            }
        }

        DBG ("TestWavExporter: Generated " + juce::String (totalFiles)
             + " files in " + outputDir.getFullPathName());

        return totalFiles;
    }

private:
    static bool writeWav (const juce::File& file,
                          const juce::AudioBuffer<float>& buffer,
                          double sampleRate)
    {
        file.deleteFile();

        auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream());
        if (stream == nullptr)
            return false;

        juce::WavAudioFormat wav;
        auto* rawStream = stream.get();

        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (rawStream,
                                 sampleRate,
                                 (unsigned int) buffer.getNumChannels(),
                                 24, {}, 0));

        if (writer == nullptr)
            return false;

        stream.release(); // writer takes ownership of the stream
        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
        return true;
    }
};

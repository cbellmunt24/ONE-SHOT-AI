/*
  ==============================================================================

   ONE-SHOT AI Generator
   Plugin JUCE que genera one-shots de audio proceduralmente con IA basada en reglas.
   10 instrumentos x 9 generos.

  ==============================================================================
*/

/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

 name:             WebViewPluginDemo
 version:          1.0.0
 vendor:           JUCE
 website:          http://juce.com
 description:      Filtering audio plugin using an HTML/JS user interface

 dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats,
                   juce_audio_plugin_client, juce_audio_processors, juce_dsp,
                   juce_audio_utils, juce_core, juce_data_structures,
                   juce_events, juce_graphics, juce_gui_basics, juce_gui_extra,
                   juce_audio_processors_headless
 exporters:        xcode_mac, vs2022, vs2026, linux_make, androidstudio,
                   xcode_iphone

 moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1, JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1

 type:             AudioProcessor
 mainClass:        WebViewPluginAudioProcessorWrapper

 useLocalCopy:     1

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

#include "DemoUtilities.h"
#include "Test/TestWavExporter.h"
#include "AI/ParameterGenerator.h"
#include "SynthEngine/SynthEngine.h"
#include "WebUI/OneShotWebUI.h"

//==============================================================================
// Helpers
//==============================================================================

static auto streamToVector (juce::InputStream& stream)
{
    std::vector<std::byte> result ((size_t) stream.getTotalLength());
    stream.setPosition (0);
    [[maybe_unused]] const auto bytesRead = stream.read (result.data(), result.size());
    jassert (bytesRead == (ssize_t) result.size());
    return result;
}

static std::vector<std::byte> encodeWav (const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    juce::MemoryBlock memBlock;

    {
        auto* memStream = new juce::MemoryOutputStream (memBlock, false);
        juce::WavAudioFormat wavFormat;

        std::unique_ptr<juce::AudioFormatWriter> writer (
            wavFormat.createWriterFor (memStream, sampleRate,
                                       (unsigned int) buffer.getNumChannels(),
                                       16, {}, 0));

        if (writer == nullptr)
        {
            delete memStream;
            return {};
        }

        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }

    std::vector<std::byte> result (memBlock.getSize());
    std::memcpy (result.data(), memBlock.getData(), memBlock.getSize());
    return result;
}

static bool writeWavToFile (const juce::File& file,
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
        wav.createWriterFor (rawStream, sampleRate,
                             (unsigned int) buffer.getNumChannels(),
                             24, {}, 0));

    if (writer == nullptr)
        return false;

    stream.release();
    writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    return true;
}

static juce::StringPairArray parseQueryParams (const juce::String& url)
{
    juce::StringPairArray params;
    auto queryStart = url.indexOfChar ('?');

    if (queryStart < 0)
        return params;

    auto query = url.substring (queryStart + 1);
    juce::StringArray pairs;
    pairs.addTokens (query, "&", "");

    for (const auto& pair : pairs)
    {
        auto eq = pair.indexOfChar ('=');

        if (eq > 0)
            params.set (pair.substring (0, eq),
                        juce::URL::removeEscapeChars (pair.substring (eq + 1)));
    }

    return params;
}

static GenerationRequest buildRequest (const juce::StringPairArray& params)
{
    GenerationRequest req;

    req.instrument = static_cast<InstrumentType> (params.getValue ("instrument", "0").getIntValue());
    req.genre      = static_cast<GenreStyle>     (params.getValue ("genre",      "0").getIntValue());
    req.attack     = static_cast<AttackType>     (params.getValue ("attack",     "0").getIntValue());
    req.energy     = static_cast<EnergyLevel>    (params.getValue ("energy",     "2").getIntValue());

    req.character.brillo     = params.getValue ("brillo",     "50").getFloatValue() / 100.0f;
    req.character.cuerpo     = params.getValue ("cuerpo",     "50").getFloatValue() / 100.0f;
    req.character.textura    = params.getValue ("textura",    "50").getFloatValue() / 100.0f;
    req.character.movimiento = params.getValue ("movimiento",  "0").getFloatValue() / 100.0f;

    req.impacto = params.getValue ("impacto", "80").getFloatValue() / 100.0f;
    req.seed    = (unsigned int) params.getValue ("seed", "12345").getIntValue();

    return req;
}

//==============================================================================
// Processor
//==============================================================================

class WebViewPluginAudioProcessor  : public juce::AudioProcessor
{
public:
    WebViewPluginAudioProcessor()
        : AudioProcessor (BusesProperties()
                        #if ! JucePlugin_IsMidiEffect
                         #if ! JucePlugin_IsSynth
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                         #endif
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                        #endif
                          )
    {
    }

    //==============================================================================
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        auto outSet = layouts.getMainOutputChannelSet();
        return outSet == juce::AudioChannelSet::mono()
            || outSet == juce::AudioChannelSet::stereo();
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;
        buffer.clear();
    }

    using AudioProcessor::processBlock;

    //==============================================================================
    const juce::String getName() const override        { return JucePlugin_Name; }

    bool acceptsMidi() const override            { return false; }
    bool producesMidi() const override           { return false; }
    bool isMidiEffect() const override           { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override                                { return 1; }
    int getCurrentProgram() override                             { return 0; }
    void setCurrentProgram (int) override                        {}
    const juce::String getProgramName (int) override             { return {}; }
    void changeProgramName (int, const juce::String&) override   {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock&) override       {}
    void setStateInformation (const void*, int) override         {}

    //==============================================================================
    ParameterGenerator paramGen;
    SynthEngine synthEngine;
    juce::AudioBuffer<float> lastGeneratedBuffer;
    juce::String lastFileName;

    static constexpr double generationSampleRate = 44100.0;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebViewPluginAudioProcessor)
};

//==============================================================================
// Drag-to-DAW native strip
//==============================================================================

class DragFileComponent  : public juce::Component
{
public:
    explicit DragFileComponent (WebViewPluginAudioProcessor& p) : proc (p) {}

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        g.setColour (juce::Colour (0xff1c1c30));
        g.fillRect (bounds);

        g.setColour (juce::Colour (0xff333355));
        g.drawLine (0.0f, 0.5f, (float) getWidth(), 0.5f, 1.0f);

        auto textArea = bounds.reduced (14, 0);
        bool hasAudio = proc.lastGeneratedBuffer.getNumSamples() > 0;

        if (hasAudio)
        {
            g.setColour (juce::Colour (0xff4ecdc4));
            g.setFont (juce::Font (13.0f).boldened());
            g.drawText (proc.lastFileName + "  |  Drag to DAW",
                        textArea, juce::Justification::centredLeft);
        }
        else
        {
            g.setColour (juce::Colour (0xff8888aa).withAlpha (0.5f));
            g.setFont (juce::Font (12.5f));
            g.drawText ("Drag to DAW",
                        textArea, juce::Justification::centredLeft);
        }
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (proc.lastGeneratedBuffer.getNumSamples() == 0)
            return;

        auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory);
        tempFile = tempDir.getChildFile (proc.lastFileName);
        writeWavToFile (tempFile, proc.lastGeneratedBuffer, proc.generationSampleRate);
        dragStarted = false;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! tempFile.existsAsFile() || dragStarted)
            return;

        if (e.getDistanceFromDragStart() > 4)
        {
            dragStarted = true;
            juce::StringArray files;
            files.add (tempFile.getFullPathName());
            juce::DragAndDropContainer::performExternalDragDropOfFiles (files, false, this);
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        dragStarted = false;
    }

    juce::MouseCursor getMouseCursor() override
    {
        if (proc.lastGeneratedBuffer.getNumSamples() > 0)
            return juce::MouseCursor::DraggingHandCursor;
        return juce::MouseCursor::NormalCursor;
    }

private:
    WebViewPluginAudioProcessor& proc;
    juce::File tempFile;
    bool dragStarted = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DragFileComponent)
};

//==============================================================================
// Forward declarations
//==============================================================================

extern const juce::String localDevServerAddress;

//==============================================================================
struct SinglePageBrowser : juce::WebBrowserComponent
{
    using WebBrowserComponent::WebBrowserComponent;

    bool pageAboutToLoad (const juce::String& newURL) override;
};

//==============================================================================
// Editor
//==============================================================================

class WebViewPluginAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    explicit WebViewPluginAudioProcessorEditor (WebViewPluginAudioProcessor&);

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    WebViewPluginAudioProcessor& processorRef;

    DragFileComponent dragComponent { processorRef };

    SinglePageBrowser webComponent {
        juce::WebBrowserComponent::Options{}
            .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                .withUserDataFolder (juce::File::getSpecialLocation (
                    juce::File::SpecialLocationType::tempDirectory)))
            .withNativeIntegrationEnabled()
            .withResourceProvider ([this] (const auto& url)
                                   {
                                       return getResource (url);
                                   },
                                   juce::URL { localDevServerAddress }.getOrigin())
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebViewPluginAudioProcessorEditor)
};

//==============================================================================
// Implementations
//==============================================================================

#if JUCE_ANDROID
const juce::String localDevServerAddress = "http://10.0.2.2:3000/";
#else
const juce::String localDevServerAddress = "http://localhost:3000/";
#endif

bool SinglePageBrowser::pageAboutToLoad (const juce::String& newURL)
{
    return newURL == localDevServerAddress || newURL == getResourceProviderRoot();
}

//==============================================================================
WebViewPluginAudioProcessorEditor::WebViewPluginAudioProcessorEditor (WebViewPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    addAndMakeVisible (dragComponent);
    addAndMakeVisible (webComponent);
    webComponent.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    setSize (480, 720);
}

//==============================================================================
void WebViewPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141420));
}

void WebViewPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    dragComponent.setBounds (bounds.removeFromBottom (44));
    webComponent.setBounds (bounds);
}

//==============================================================================
std::optional<juce::WebBrowserComponent::Resource>
WebViewPluginAudioProcessorEditor::getResource (const juce::String& url)
{
    const auto urlToRetrieve = url == "/" ? juce::String { "index.html" }
                                          : url.fromFirstOccurrenceOf ("/", false, false);

    // ── Serve the UI ──
    if (urlToRetrieve == "index.html")
    {
        const char* html = OneShotWebUI::getHTML();
        size_t htmlLen = OneShotWebUI::getHTMLSize();
        juce::MemoryInputStream stream (html, htmlLen, false);
        return juce::WebBrowserComponent::Resource { streamToVector (stream),
                                                      juce::String { "text/html" } };
    }

    // ── API: Generate one-shot ──
    if (urlToRetrieve.startsWith ("api/generate"))
    {
        auto params = parseQueryParams (urlToRetrieve);
        auto req = buildRequest (params);

        auto result = processorRef.paramGen.generate (req, req.seed);
        auto buffer = processorRef.synthEngine.generate (result,
                                                          processorRef.generationSampleRate,
                                                          req.seed);

        processorRef.lastGeneratedBuffer = buffer;

        // Build filename for drag-to-DAW strip
        {
            const char* instNames[]  = { "Kick", "Snare", "HiHat", "Clap", "Perc", "Bass808", "Lead", "Pluck", "Pad", "Texture" };
            const char* genreNames[] = { "Trap", "HipHop", "Techno", "House", "Reggaeton", "Afrobeat", "RnB", "EDM", "Ambient" };
            int instIdx  = juce::jlimit (0, 9, params.getValue ("instrument", "0").getIntValue());
            int genreIdx = juce::jlimit (0, 8, params.getValue ("genre",      "0").getIntValue());
            processorRef.lastFileName = juce::String (instNames[instIdx]) + "_"
                                      + genreNames[genreIdx] + "_"
                                      + params.getValue ("seed", "12345") + ".wav";
        }
        dragComponent.repaint();

        auto wavData = encodeWav (buffer, processorRef.generationSampleRate);

        if (wavData.empty())
            return std::nullopt;

        return juce::WebBrowserComponent::Resource { std::move (wavData),
                                                      juce::String { "audio/wav" } };
    }

    // ── API: Export last generated buffer to file ──
    if (urlToRetrieve.startsWith ("api/export"))
    {
        auto params = parseQueryParams (urlToRetrieve);
        auto name = params.getValue ("name", "OneShot");

        auto desktop = juce::File::getSpecialLocation (juce::File::userDesktopDirectory);
        auto outputDir = desktop.getChildFile ("ONE-SHOT AI Output");
        outputDir.createDirectory();

        auto file = outputDir.getChildFile (name + ".wav");

        juce::String jsonResponse;

        if (processorRef.lastGeneratedBuffer.getNumSamples() > 0
            && writeWavToFile (file, processorRef.lastGeneratedBuffer,
                               processorRef.generationSampleRate))
        {
            jsonResponse = "{\"ok\":true,\"path\":\"" + file.getFullPathName().replace ("\\", "\\\\") + "\"}";
        }
        else
        {
            jsonResponse = "{\"ok\":false,\"error\":\"No audio to export\"}";
        }

        juce::MemoryInputStream stream (jsonResponse.getCharPointer(),
                                         jsonResponse.getNumBytesAsUTF8(), false);
        return juce::WebBrowserComponent::Resource { streamToVector (stream),
                                                      juce::String { "application/json" } };
    }

    return std::nullopt;
}

//==============================================================================
// Wrapper (entry point)
//==============================================================================

class WebViewPluginAudioProcessorWrapper  : public WebViewPluginAudioProcessor
{
public:
    WebViewPluginAudioProcessorWrapper()
    {
        // === TEST: Genera .wav de prueba al arrancar ===
        auto desktop = juce::File::getSpecialLocation (juce::File::userDesktopDirectory);
        auto testDir = desktop.getChildFile ("ONE-SHOT AI Test Output");
        int count = TestWavExporter::exportAll (testDir);
        DBG ("ONE-SHOT AI: Exported " + juce::String (count) + " test wavs to Desktop");
    }

    bool hasEditor() const override               { return true; }
    juce::AudioProcessorEditor* createEditor() override { return new WebViewPluginAudioProcessorEditor (*this); }
};

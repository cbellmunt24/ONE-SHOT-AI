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
#include "OneShotMatch/OneShotMatchEngine.h"

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
#if USE_ONNX_INFERENCE
        // Locate the Resources folder next to the plugin binary
        auto pluginFile = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
        auto resourcesDir = pluginFile.getParentDirectory().getChildFile ("Resources");

        // Fallback: walk up from the exe to find the project's Resources folder.
        // Standalone: Builds/VisualStudio20XX/x64/Release/Standalone Plugin/exe
        //   → 5 levels up = ONE-SHOT AI/ → Resources/
        if (! resourcesDir.isDirectory())
        {
            auto dir = pluginFile.getParentDirectory();
            for (int i = 0; i < 6 && ! resourcesDir.isDirectory(); ++i)
            {
                resourcesDir = dir.getChildFile ("Resources");
                dir = dir.getParentDirectory();
            }
        }

        if (resourcesDir.isDirectory())
        {
            bool loaded = paramGen.loadONNXModels (resourcesDir.getFullPathName().toStdString());
            if (loaded)
                DBG ("ONNX models loaded from: " + resourcesDir.getFullPathName());
            else
                DBG ("ONNX model loading failed: " + juce::String (paramGen.getONNXError()));
        }
        else
        {
            DBG ("Resources directory not found, using rule-based generation");
        }
#endif
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

    // One-Shot Match system (independent from generator)
    oneshotmatch::OneShotMatchEngine matchEngine;

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

    // ── API: One-Shot Match — Load reference audio ──
    if (urlToRetrieve.startsWith ("api/match/load"))
    {
        auto params = parseQueryParams (urlToRetrieve);
        auto b64Data = params.getValue ("data", "");
        auto filePath = params.getValue ("path", "");

        juce::String jsonResponse;

        if (b64Data.isNotEmpty())
        {
            // Decode base64 audio data from WebView2
            juce::MemoryBlock decoded;
            juce::MemoryOutputStream decodedStream (decoded, false);
            if (juce::Base64::convertFromBase64 (decodedStream, b64Data))
            {
                // Parse WAV/audio from decoded bytes
                juce::AudioFormatManager formatManager;
                formatManager.registerBasicFormats();

                auto memStream = std::make_unique<juce::MemoryInputStream> (
                    decoded.getData(), decoded.getSize(), false);

                std::unique_ptr<juce::AudioFormatReader> reader (
                    formatManager.createReaderFor (std::move (memStream)));

                if (reader != nullptr)
                {
                    juce::AudioBuffer<float> buffer ((int) reader->numChannels,
                                                      (int) reader->lengthInSamples);
                    reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, true);

                    if (processorRef.matchEngine.loadReference (buffer, reader->sampleRate))
                        jsonResponse = "{\"ok\":true}";
                    else
                        jsonResponse = "{\"ok\":false,\"error\":\"" + processorRef.matchEngine.getError() + "\"}";
                }
                else
                {
                    jsonResponse = "{\"ok\":false,\"error\":\"Could not decode audio format\"}";
                }
            }
            else
            {
                jsonResponse = "{\"ok\":false,\"error\":\"Base64 decode failed\"}";
            }
        }
        else if (filePath.isNotEmpty())
        {
            juce::File file (filePath);
            if (processorRef.matchEngine.loadReference (file))
                jsonResponse = "{\"ok\":true}";
            else
                jsonResponse = "{\"ok\":false,\"error\":\"" + processorRef.matchEngine.getError() + "\"}";
        }
        else
        {
            jsonResponse = "{\"ok\":false,\"error\":\"No audio data provided\"}";
        }

        juce::MemoryInputStream stream (jsonResponse.getCharPointer(),
                                         jsonResponse.getNumBytesAsUTF8(), false);
        return juce::WebBrowserComponent::Resource { streamToVector (stream),
                                                      juce::String { "application/json" } };
    }

    // ── API: One-Shot Match — Analyze reference ──
    if (urlToRetrieve.startsWith ("api/match/analyze"))
    {
        processorRef.matchEngine.analyzeReference();
        auto json = processorRef.matchEngine.descriptorsToJSON (
            processorRef.matchEngine.getRefDescriptors());

        juce::MemoryInputStream stream (json.getCharPointer(), json.getNumBytesAsUTF8(), false);
        return juce::WebBrowserComponent::Resource { streamToVector (stream),
                                                      juce::String { "application/json" } };
    }

    // ── API: One-Shot Match — Start matching ──
    if (urlToRetrieve.startsWith ("api/match/start"))
    {
        processorRef.matchEngine.startMatch();
        juce::String json = "{\"ok\":true}";
        juce::MemoryInputStream stream (json.getCharPointer(), json.getNumBytesAsUTF8(), false);
        return juce::WebBrowserComponent::Resource { streamToVector (stream),
                                                      juce::String { "application/json" } };
    }

    // ── API: One-Shot Match — Poll status ──
    if (urlToRetrieve.startsWith ("api/match/status"))
    {
        auto state = processorRef.matchEngine.getState();
        const char* stateStr = "idle";
        switch (state)
        {
            case oneshotmatch::MatchState::Idle:      stateStr = "idle"; break;
            case oneshotmatch::MatchState::Analyzing:  stateStr = "analyzing"; break;
            case oneshotmatch::MatchState::Matching:   stateStr = "matching"; break;
            case oneshotmatch::MatchState::Done:       stateStr = "done"; break;
            case oneshotmatch::MatchState::Error:      stateStr = "error"; break;
        }

        auto& engine = processorRef.matchEngine;

        juce::String json = "{";
        json += "\"state\":\"" + juce::String (stateStr) + "\",";
        json += "\"iteration\":" + juce::String (engine.getIteration()) + ",";
        json += "\"maxIterations\":" + juce::String (engine.getMaxIterations()) + ",";
        json += "\"distance\":" + juce::String (engine.getDistance(), 4);

        if (state == oneshotmatch::MatchState::Done)
        {
            json += ",\"params\":" + engine.paramsToJSON (engine.getBestParams());
            json += ",\"sensitivity\":" + engine.sensitivityToJSON();
            json += ",\"refDescriptors\":" + engine.descriptorsToJSON (engine.getRefDescriptors());
            json += ",\"matchedDescriptors\":" + engine.descriptorsToJSON (engine.getMatchedDescriptors());
            json += ",\"converged\":" + juce::String (engine.getOptResult().converged ? "true" : "false");
            json += ",\"gapAnalysis\":" + engine.gapAnalysisToJSON();

            // Buffer diagnostics
            auto& mbuf = engine.getMatchedBuffer();
            if (mbuf.getNumSamples() > 0)
            {
                float pk = mbuf.getMagnitude (0, 0, mbuf.getNumSamples());
                float rms = 0.0f;
                const float* d = mbuf.getReadPointer (0);
                for (int s = 0; s < mbuf.getNumSamples(); ++s) rms += d[s] * d[s];
                rms = std::sqrt (rms / (float) mbuf.getNumSamples());
                json += ",\"bufPeak\":" + juce::String (pk, 4);
                json += ",\"bufRMS\":" + juce::String (rms, 4);
                json += ",\"bufSamples\":" + juce::String (mbuf.getNumSamples());
                json += ",\"bufSR\":" + juce::String (engine.getSampleRate(), 0);
                json += ",\"bufDurationMs\":" + juce::String (1000.0 * mbuf.getNumSamples() / engine.getSampleRate(), 1);
            }
        }

        if (state == oneshotmatch::MatchState::Error)
            json += ",\"error\":\"" + engine.getError() + "\"";

        json += "}";

        juce::MemoryInputStream stream (json.getCharPointer(), json.getNumBytesAsUTF8(), false);
        return juce::WebBrowserComponent::Resource { streamToVector (stream),
                                                      juce::String { "application/json" } };
    }

    // ── API: One-Shot Match — Cancel ──
    if (urlToRetrieve.startsWith ("api/match/cancel"))
    {
        processorRef.matchEngine.cancelMatch();
        juce::String json = "{\"ok\":true}";
        juce::MemoryInputStream stream (json.getCharPointer(), json.getNumBytesAsUTF8(), false);
        return juce::WebBrowserComponent::Resource { streamToVector (stream),
                                                      juce::String { "application/json" } };
    }

    // ── API: One-Shot Match — Get matched audio as WAV ──
    if (urlToRetrieve.startsWith ("api/match/audio"))
    {
        auto& buf = processorRef.matchEngine.getMatchedBuffer();
        if (buf.getNumSamples() > 0)
        {
            auto wavData = encodeWav (buf, processorRef.matchEngine.getSampleRate());
            if (! wavData.empty())
                return juce::WebBrowserComponent::Resource { std::move (wavData),
                                                              juce::String { "audio/wav" } };
        }
        return std::nullopt;
    }

    // ── API: One-Shot Match — Export audio to file ──
    if (urlToRetrieve.startsWith ("api/match/export-audio"))
    {
        auto desktop = juce::File::getSpecialLocation (juce::File::userDesktopDirectory);
        auto outputDir = desktop.getChildFile ("ONE-SHOT AI Output");
        outputDir.createDirectory();

        auto refName = processorRef.matchEngine.getReferenceFile().getFileNameWithoutExtension();
        if (refName.isEmpty()) refName = "OneShotMatch";
        auto file = outputDir.getChildFile (refName + "_matched.wav");

        juce::String json;
        if (processorRef.matchEngine.exportAudio (file))
            json = "{\"ok\":true,\"path\":\"" + file.getFullPathName().replace ("\\", "\\\\") + "\"}";
        else
            json = "{\"ok\":false,\"error\":\"Export failed\"}";

        juce::MemoryInputStream stream (json.getCharPointer(), json.getNumBytesAsUTF8(), false);
        return juce::WebBrowserComponent::Resource { streamToVector (stream),
                                                      juce::String { "application/json" } };
    }

    // ── API: One-Shot Match — Export preset JSON ──
    if (urlToRetrieve.startsWith ("api/match/export-preset"))
    {
        auto desktop = juce::File::getSpecialLocation (juce::File::userDesktopDirectory);
        auto outputDir = desktop.getChildFile ("ONE-SHOT AI Output");
        outputDir.createDirectory();

        auto refName = processorRef.matchEngine.getReferenceFile().getFileNameWithoutExtension();
        if (refName.isEmpty()) refName = "OneShotMatch";
        auto file = outputDir.getChildFile (refName + "_preset.json");

        auto presetJson = processorRef.matchEngine.exportPresetJSON();
        file.replaceWithText (presetJson);

        juce::String json = "{\"ok\":true,\"path\":\"" + file.getFullPathName().replace ("\\", "\\\\") + "\"}";
        juce::MemoryInputStream stream (json.getCharPointer(), json.getNumBytesAsUTF8(), false);
        return juce::WebBrowserComponent::Resource { streamToVector (stream),
                                                      juce::String { "application/json" } };
    }

    // ── API: One-Shot Match — Export TXT report ──
    if (urlToRetrieve.startsWith ("api/match/export-txt"))
    {
        auto desktop = juce::File::getSpecialLocation (juce::File::userDesktopDirectory);
        auto outputDir = desktop.getChildFile ("ONE-SHOT AI Output");
        outputDir.createDirectory();

        auto refName = processorRef.matchEngine.getReferenceFile().getFileNameWithoutExtension();
        if (refName.isEmpty()) refName = "OneShotMatch";
        auto file = outputDir.getChildFile (refName + "_report.txt");

        auto& eng = processorRef.matchEngine;
        auto& refD = eng.getRefDescriptors();
        auto& matD = eng.getMatchedDescriptors();
        auto& res  = eng.getOptResult();
        auto& bp   = eng.getBestParams();

        juce::String txt;
        txt += "=== ONE-SHOT MATCH REPORT ===\n\n";
        txt += "Reference: " + eng.getReferenceFile().getFileName() + "\n";
        txt += "Distance: " + juce::String (res.bestDistance, 4) + "\n";
        txt += "Score: " + juce::String ((int) std::round (100.0f / (1.0f + res.bestDistance * 0.8f))) + "%\n";
        txt += "Iterations: " + juce::String (res.iterations) + "\n";
        txt += "Converged: " + juce::String (res.converged ? "yes" : "no") + "\n";
        txt += "Extensions: " + juce::String (res.extensionsActivated) + "\n\n";

        txt += "--- DESCRIPTORS (ref vs matched) ---\n";
        txt += "fundamentalFreq: " + juce::String (refD.fundamentalFreq, 1) + " vs " + juce::String (matD.fundamentalFreq, 1) + " Hz\n";
        txt += "pitchStart: " + juce::String (refD.pitchStart, 1) + " vs " + juce::String (matD.pitchStart, 1) + " Hz\n";
        txt += "pitchDropST: " + juce::String (refD.pitchDropSemitones, 1) + " vs " + juce::String (matD.pitchDropSemitones, 1) + " st\n";
        txt += "pitchDropTime: " + juce::String (refD.pitchDropTime * 1000, 1) + " vs " + juce::String (matD.pitchDropTime * 1000, 1) + " ms\n";
        txt += "attackTime: " + juce::String (refD.attackTime * 1000, 2) + " vs " + juce::String (matD.attackTime * 1000, 2) + " ms\n";
        txt += "decayTime: " + juce::String (refD.decayTime * 1000, 1) + " vs " + juce::String (matD.decayTime * 1000, 1) + " ms\n";
        txt += "transientStr: " + juce::String (refD.transientStrength, 2) + " vs " + juce::String (matD.transientStrength, 2) + "\n";
        txt += "spectralCentroid: " + juce::String (refD.spectralCentroid, 0) + " vs " + juce::String (matD.spectralCentroid, 0) + " Hz\n";
        txt += "brightness: " + juce::String (refD.brightness, 3) + " vs " + juce::String (matD.brightness, 3) + "\n";
        txt += "HNR: " + juce::String (refD.harmonicNoiseRatio, 3) + " vs " + juce::String (matD.harmonicNoiseRatio, 3) + "\n";
        txt += "subEnergy: " + juce::String (refD.subEnergy, 3) + " vs " + juce::String (matD.subEnergy, 3) + "\n";
        txt += "lowMidEnergy: " + juce::String (refD.lowMidEnergy, 3) + " vs " + juce::String (matD.lowMidEnergy, 3) + "\n";
        txt += "midEnergy: " + juce::String (refD.midEnergy, 3) + " vs " + juce::String (matD.midEnergy, 3) + "\n";
        txt += "highEnergy: " + juce::String (refD.highEnergy, 3) + " vs " + juce::String (matD.highEnergy, 3) + "\n";
        txt += "rmsLoudness: " + juce::String (refD.rmsLoudness, 4) + " vs " + juce::String (matD.rmsLoudness, 4) + "\n";
        txt += "totalDuration: " + juce::String (refD.totalDuration * 1000, 1) + " vs " + juce::String (matD.totalDuration * 1000, 1) + " ms\n\n";

        txt += "--- SYNTH PARAMS ---\n";
        float arr[oneshotmatch::MatchSynthParams::NUM_PARAMS];
        bp.toArray (arr);
        for (int i = 0; i < oneshotmatch::MatchSynthParams::NUM_PARAMS; ++i)
        {
            txt += juce::String (oneshotmatch::MatchSynthParams::getParamName (i)).paddedRight (' ', 22);
            txt += juce::String (arr[i], 4);
            txt += " " + juce::String (oneshotmatch::MatchSynthParams::getParamUnit (i));
            if (res.sensitivity[i] > 0.1f)
                txt += "  [sens=" + juce::String (res.sensitivity[i], 2) + "]";
            txt += "\n";
        }

        file.replaceWithText (txt);

        juce::String json = "{\"ok\":true,\"path\":\"" + file.getFullPathName().replace ("\\", "\\\\") + "\"}";
        juce::MemoryInputStream stream (json.getCharPointer(), json.getNumBytesAsUTF8(), false);
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

        // === MANUAL PARAMS TEST — export both WAVs for comparison ===
        {
            juce::File kickFile ("C:\\Users\\charl\\Desktop\\ESMUC\\# SONOLOGIA ESMUC\\LABSO II\\ONE-SHOT AI\\Training\\libraries\\kicks\\trap\\kicks_trap_0027.wav");
            if (kickFile.existsAsFile() && matchEngine.loadReference (kickFile))
            {
                matchEngine.analyzeReference();
                auto& refD = matchEngine.getRefDescriptors();
                auto& refBuf = matchEngine.getReferenceBuffer();

                // Export reference WAV
                auto outputDir = desktop.getChildFile ("ONE-SHOT AI Output");
                outputDir.createDirectory();
                writeWavToFile (outputDir.getChildFile ("ref_kick.wav"), refBuf, 44100.0);

                // Generate with manual params (the ones that gave perfect descriptors)
                oneshotmatch::MatchSynthParams manual;
                manual.oscType = 12;  // wavetable — captures exact timbre of reference
                manual.basePitch = 54.2f;
                manual.bodyHarmonics = 0.1f;      // minimal — just enough warmth
                manual.pitchEnvDepth = 10.6f;
                manual.pitchEnvFast = 0.002f;
                manual.pitchEnvSlow = 0.030f;
                manual.pitchEnvBalance = 0.6f;
                manual.ampAttack = 0.0001f;
                manual.ampPunchDecay = 0.015f;
                manual.ampBodyDecay = 0.045f;
                manual.ampPunchLevel = 0.7f;
                manual.subLevel = 0.0f;          // NO sub — body handles everything
                manual.subTailDecay = 0.05f;
                manual.subDetune = 0.0f;
                manual.clickAmount = 0.0f;
                manual.distortion = 0.05f;        // very light — clean kick
                manual.noiseAmount = 0.0f;
                manual.filterCutoff = 400.0f;
                manual.harmonicEmphasis = 0.1f;
                manual.bodyMix = 1.0f;
                manual.subMix = 0.0f;
                manual.clickMix = 0.0f;
                manual.topMix = 0.0f;
                manual.subCrossover = 0.0f;     // NO crossover for sine
                manual.envSustainLevel = 0.9f;   // strong sustain plateau
                manual.envSustainTime  = 0.040f; // 40ms plateau
                manual.envRelease      = 0.010f; // 10ms sharp release
                // Compressor: smooths out amplitude oscillations
                manual.compAmount = 0.8f;
                manual.compRatio = 8.0f;
                manual.compAttack = 0.0002f;     // 0.2ms — instant
                manual.compRelease = 0.02f;      // 20ms
                // Limiter in synth handles peak flattening now — no need for masterSat

                oneshotmatch::OneShotMatchSynth synth;
                synth.setWavetable (&matchEngine.getRefWavetable());
                auto manualBuf = synth.generate (manual, 44100.0);
                writeWavToFile (outputDir.getChildFile ("manual_match.wav"), manualBuf, 44100.0);

                // Also run the optimizer
                DBG ("AUTOTEST: Starting optimizer match...");
                matchEngine.startMatch();
                int waitMs = 0;
                while (matchEngine.getState() == oneshotmatch::MatchState::Matching && waitMs < 300000)
                {
                    juce::Thread::sleep (500);
                    waitMs += 500;
                }

                DBG ("AUTOTEST: Match completed in " + juce::String (waitMs / 1000) + "s");

                // Extract results
                auto& bestParams = matchEngine.getBestParams();
                auto& bestBuffer = matchEngine.getMatchedBuffer();
                float bestDist = matchEngine.getDistance();
                int bestScore = (int) std::round (100.0f / (1.0f + bestDist * 0.8f));

                // Export optimizer match WAV for comparison
                if (bestBuffer.getNumSamples() > 0)
                    writeWavToFile (outputDir.getChildFile ("optimizer_match.wav"), bestBuffer, 44100.0);

                // === WAVEFORM COMPARISON: ref vs manual vs optimizer ===
                // Dump peak amplitude per 1ms window for first 50ms + every 50ms after
                {
                    auto dumpEnvelope = [](const juce::AudioBuffer<float>& buf, float sr) -> juce::String
                    {
                        juce::String s;
                        const float* data = buf.getReadPointer (0);
                        int total = buf.getNumSamples();
                        // First 50ms: 1ms windows
                        for (int ms = 0; ms < 50 && ms * (int)(sr / 1000) < total; ++ms)
                        {
                            int start = ms * (int)(sr / 1000);
                            int end = std::min (start + (int)(sr / 1000), total);
                            float pk = 0.0f, rms = 0.0f;
                            for (int i = start; i < end; ++i) { pk = std::max (pk, std::abs (data[i])); rms += data[i] * data[i]; }
                            rms = std::sqrt (rms / (float)(end - start));
                            s += juce::String (ms) + "ms: pk=" + juce::String (pk, 3) + " rms=" + juce::String (rms, 3) + "\n";
                        }
                        // Then every 50ms up to 800ms
                        for (int ms = 50; ms < 800; ms += 50)
                        {
                            int start = ms * (int)(sr / 1000);
                            int end = std::min (start + (int)(sr / 1000), total);
                            if (start >= total) break;
                            float pk = 0.0f;
                            for (int i = start; i < end; ++i) pk = std::max (pk, std::abs (data[i]));
                            s += juce::String (ms) + "ms: pk=" + juce::String (pk, 3) + "\n";
                        }
                        return s;
                    };

                    juce::String wf;
                    wf += "=== WAVEFORM ENVELOPE COMPARISON ===\n\n";
                    wf += "--- REFERENCE ---\n" + dumpEnvelope (refBuf, 44100.0f) + "\n";
                    wf += "--- MANUAL ---\n" + dumpEnvelope (manualBuf, 44100.0f) + "\n";
                    wf += "--- OPTIMIZER ---\n" + dumpEnvelope (bestBuffer, 44100.0f) + "\n";
                    outputDir.getChildFile ("waveform_compare.txt").replaceWithText (wf);
                }

                // Analyze the best output
                oneshotmatch::DescriptorExtractor ext;
                oneshotmatch::MatchDescriptors optD;
                if (bestBuffer.getNumSamples() > 0)
                    optD = ext.extract (bestBuffer, 44100.0);

                juce::String txt;
                txt += "=== OPTIMIZER AUTO-TEST ===\n";
                txt += "Kick: kicks_trap_0027.wav\n";
                txt += "Time: " + juce::String (waitMs / 1000) + "s\n";
                txt += "Distance: " + juce::String (bestDist, 4) + "\n";
                txt += "Score: " + juce::String (bestScore) + "%\n\n";

                txt += "--- KEY PARAMS ---\n";
                txt += "oscType: " + juce::String ((int) bestParams.oscType) + "\n";
                txt += "basePitch: " + juce::String (bestParams.basePitch, 1) + "\n";
                txt += "bodyHarmonics: " + juce::String (bestParams.bodyHarmonics, 3) + "\n";
                txt += "pitchEnvDepth: " + juce::String (bestParams.pitchEnvDepth, 1) + "\n";
                txt += "pitchEnvFast: " + juce::String (bestParams.pitchEnvFast, 4) + "\n";
                txt += "pitchEnvSlow: " + juce::String (bestParams.pitchEnvSlow, 4) + "\n";
                txt += "ampAttack: " + juce::String (bestParams.ampAttack, 4) + "\n";
                txt += "ampPunchDecay: " + juce::String (bestParams.ampPunchDecay, 4) + "\n";
                txt += "ampBodyDecay: " + juce::String (bestParams.ampBodyDecay, 3) + "\n";
                txt += "subLevel: " + juce::String (bestParams.subLevel, 3) + "\n";
                txt += "subTailDecay: " + juce::String (bestParams.subTailDecay, 3) + "\n";
                txt += "subMix: " + juce::String (bestParams.subMix, 3) + "\n";
                txt += "bodyMix: " + juce::String (bestParams.bodyMix, 3) + "\n";
                txt += "clickAmount: " + juce::String (bestParams.clickAmount, 3) + "\n";
                txt += "clickMix: " + juce::String (bestParams.clickMix, 3) + "\n";
                txt += "topMix: " + juce::String (bestParams.topMix, 3) + "\n";
                txt += "noiseAmount: " + juce::String (bestParams.noiseAmount, 3) + "\n";
                txt += "filterCutoff: " + juce::String (bestParams.filterCutoff, 0) + "\n";
                txt += "distortion: " + juce::String (bestParams.distortion, 3) + "\n";
                txt += "subCrossover: " + juce::String (bestParams.subCrossover, 1) + "\n";
                txt += "harmonicEmphasis: " + juce::String (bestParams.harmonicEmphasis, 3) + "\n\n";

                txt += "--- DESCRIPTORS (ref / optimized) ---\n";
                txt += "fundamentalFreq: " + juce::String (refD.fundamentalFreq, 1) + " / " + juce::String (optD.fundamentalFreq, 1) + "\n";
                txt += "pitchStart: " + juce::String (refD.pitchStart, 1) + " / " + juce::String (optD.pitchStart, 1) + "\n";
                txt += "pitchDropST: " + juce::String (refD.pitchDropSemitones, 1) + " / " + juce::String (optD.pitchDropSemitones, 1) + "\n";
                txt += "pitchDropTime: " + juce::String (refD.pitchDropTime * 1000, 1) + " / " + juce::String (optD.pitchDropTime * 1000, 1) + " ms\n";
                txt += "attackTime: " + juce::String (refD.attackTime * 1000, 2) + " / " + juce::String (optD.attackTime * 1000, 2) + " ms\n";
                txt += "decayTime: " + juce::String (refD.decayTime * 1000, 1) + " / " + juce::String (optD.decayTime * 1000, 1) + " ms\n";
                txt += "totalDuration: " + juce::String (refD.totalDuration * 1000, 0) + " / " + juce::String (optD.totalDuration * 1000, 0) + " ms\n";
                txt += "transientStr: " + juce::String (refD.transientStrength, 2) + " / " + juce::String (optD.transientStrength, 2) + "\n";
                txt += "spectralCentroid: " + juce::String (refD.spectralCentroid, 0) + " / " + juce::String (optD.spectralCentroid, 0) + "\n";
                txt += "spectralRolloff: " + juce::String (refD.spectralRolloff, 0) + " / " + juce::String (optD.spectralRolloff, 0) + "\n";
                txt += "brightness: " + juce::String (refD.brightness, 4) + " / " + juce::String (optD.brightness, 4) + "\n";
                txt += "HNR: " + juce::String (refD.harmonicNoiseRatio, 3) + " / " + juce::String (optD.harmonicNoiseRatio, 3) + "\n";
                txt += "subEnergy: " + juce::String (refD.subEnergy, 3) + " / " + juce::String (optD.subEnergy, 3) + "\n";
                txt += "lowMidEnergy: " + juce::String (refD.lowMidEnergy, 3) + " / " + juce::String (optD.lowMidEnergy, 3) + "\n";
                txt += "midEnergy: " + juce::String (refD.midEnergy, 3) + " / " + juce::String (optD.midEnergy, 3) + "\n";
                txt += "highEnergy: " + juce::String (refD.highEnergy, 4) + " / " + juce::String (optD.highEnergy, 4) + "\n";
                txt += "rmsLoudness: " + juce::String (refD.rmsLoudness, 4) + " / " + juce::String (optD.rmsLoudness, 4) + "\n";

                desktop.getChildFile ("match_autotest.txt").replaceWithText (txt);
                DBG ("AUTOTEST: dist=" + juce::String (bestDist, 3) + " score=" + juce::String (bestScore) + "%");
            }
        }
    }

    bool hasEditor() const override               { return true; }
    juce::AudioProcessorEditor* createEditor() override { return new WebViewPluginAudioProcessorEditor (*this); }
};

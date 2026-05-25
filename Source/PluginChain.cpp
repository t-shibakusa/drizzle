#include "PluginChain.h"
#include "TrackMixerProcessor.h"

class GainAudioProcessor final : public juce::AudioProcessor
{
public:
    GainAudioProcessor()
        : juce::AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                                   .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    {
    }

    const juce::String getName() const override { return "Gain"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet()
            && ! layouts.getMainOutputChannelSet().isDisabled();
    }

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        buffer.applyGain (gain.load());
    }

    using juce::AudioProcessor::processBlock;

    std::atomic<float> gain { 1.0f };
};

namespace
{
constexpr int numChannels = 2;

bool isWavesShellGateway (const juce::PluginDescription& description)
{
    return description.name.containsIgnoreCase ("WaveShell");
}

bool isWavesInternalBundlePath (const juce::PluginDescription& description)
{
    return description.fileOrIdentifier.containsIgnoreCase ("Waves\\Plug-Ins")
        || description.fileOrIdentifier.containsIgnoreCase ("Waves/Plug-Ins");
}

bool isUiSelectablePlugin (const juce::PluginDescription& description)
{
    return ! isWavesShellGateway (description)
        && ! isWavesInternalBundlePath (description);
}

void scanWaveShellFiles (juce::KnownPluginList& knownPluginList,
                         juce::AudioPluginFormat& format,
                         const juce::String& wildcard,
                         const juce::FileSearchPath& searchPaths)
{
    for (int i = 0; i < searchPaths.getNumPaths(); ++i)
    {
        const juce::File directory (searchPaths[i]);

        if (! directory.isDirectory())
            continue;

        for (const auto& shellFile : directory.findChildFiles (juce::File::findFiles, false, wildcard))
        {
            juce::OwnedArray<juce::PluginDescription> typesFound;
            format.findAllTypesForFile (typesFound, shellFile.getFullPathName());

            for (const auto* type : typesFound)
                knownPluginList.addType (*type);
        }
    }

    const juce::File vst3Directory ("C:\\Program Files\\Common Files\\VST3");

    if (wildcard.contains ("vst3") && vst3Directory.isDirectory())
    {
        for (const auto& shellFile : vst3Directory.findChildFiles (juce::File::findFiles, false, "WaveShell*.vst3"))
        {
            juce::OwnedArray<juce::PluginDescription> typesFound;
            format.findAllTypesForFile (typesFound, shellFile.getFullPathName());

            for (const auto* type : typesFound)
                knownPluginList.addType (*type);
        }
    }
}

void showPluginLoadError (const juce::String& error, const juce::PluginDescription& description)
{
    juce::String message = error;

    if (isWavesShellGateway (description))
    {
        message = juce::String::fromUTF8 (
            "WaveShell \u672c\u4f53\u306f\u30ed\u30fc\u30c9\u3067\u304d\u307e\u305b\u3093\u3002"
            "\u30ea\u30b9\u30c8\u304b\u3089\u500b\u5225\u306e Waves \u30d7\u30e9\u30b0\u30a4\u30f3\u3092\u9078\u3093\u3067 Load \u3057\u3066\u304f\u3060\u3055\u3044\u3002");
    }
    else if (description.manufacturerName.containsIgnoreCase ("Waves")
             || description.name.containsIgnoreCase ("Waves")
             || error.containsIgnoreCase ("license")
             || error.containsIgnoreCase ("licence"))
    {
        message << juce::String::fromUTF8 (
            "\n\n[Waves \u30e9\u30a4\u30bb\u30f3\u30b9\u30a8\u30e9\u30fc]\n"
            "1. Waves Central \u3092\u8d77\u52d5\u3057\u3001\u30ed\u30b0\u30a4\u30f3\u30fb\u30e9\u30a4\u30bb\u30f3\u30b9\u540c\u671f\n"
            "2. \u30ea\u30b9\u30c8\u304b\u3089 WaveShell \u3067\u306f\u306a\u304f\u500b\u5225\u30d7\u30e9\u30b0\u30a4\u30f3\u540d\u3092\u9078\u3076\n"
            "3. Browse \u3067 Plug-Ins V16 \u5185\u306e .vst3 \u3092\u76f4\u63a5\u9078\u3070\u306a\u3044\n"
            "   (Common Files\\VST3 \u306e WaveShell \u7d4c\u7531)\n"
            "4. Scan \u5f8c\u306b\u518d\u30ed\u30fc\u30c9\n"
            "5. \u6539\u5584\u3057\u306a\u3044\u5834\u5408: Waves Central \u3067\u4fee\u5fa9/\u518d\u30a4\u30f3\u30b9\u30c8\u30fc\u30eb\n\n")
            << error;
    }

    juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Plugin Load Failed",
                                                message);
}

const juce::PluginDescription* pickPreferredPlugin (const juce::OwnedArray<juce::PluginDescription>& typesFound)
{
    for (const auto* type : typesFound)
        if (type != nullptr && isUiSelectablePlugin (*type))
            return type;

    return typesFound.getFirst();
}

void connectChannel (juce::AudioProcessorGraph& graph,
                     juce::AudioProcessorGraph::NodeID source,
                     juce::AudioProcessorGraph::NodeID dest,
                     int channel)
{
    graph.addConnection ({ { source, channel }, { dest, channel } },
                         juce::AudioProcessorGraph::UpdateKind::none);
}
} // namespace

PluginChain::PluginChain()
    : deadMansPedalFile (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                             .getChildFile ("Drizzle")
                             .getChildFile ("dead_mans_pedal.txt"))
{
    formatManager = std::make_unique<juce::AudioPluginFormatManager>();
    juce::addDefaultFormatsToManager (*formatManager);
    deadMansPedalFile.getParentDirectory().createDirectory();

    ensureIoNodes();
    rebuildConnections();
}

PluginChain::~PluginChain()
{
    releaseAll();
    graph.clear();
}

void PluginChain::ensureIoNodes()
{
    if (inputNode == nullptr)
    {
        inputNode = graph.addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
            juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));
    }

    if (outputNode == nullptr)
    {
        outputNode = graph.addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
            juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
    }

    if (trackMixerNode == nullptr)
    {
        auto trackMixerInstance = std::make_unique<TrackMixerProcessor>();
        trackMixerProcessor = trackMixerInstance.get();
        trackMixerNode = graph.addNode (std::move (trackMixerInstance));
    }

    if (gainNode == nullptr)
    {
        auto gainProcessorInstance = std::make_unique<GainAudioProcessor>();
        gainProcessor = gainProcessorInstance.get();
        gainNode = graph.addNode (std::move (gainProcessorInstance));
    }
}

TrackMixerProcessor& PluginChain::getTrackMixer() noexcept
{
    jassert (trackMixerProcessor != nullptr);
    return *trackMixerProcessor;
}

const TrackMixerProcessor& PluginChain::getTrackMixer() const noexcept
{
    jassert (trackMixerProcessor != nullptr);
    return *trackMixerProcessor;
}

void PluginChain::rebuildConnections()
{
    graph.removeIllegalConnections();

    for (const auto& connection : graph.getConnections())
        graph.removeConnection (connection, juce::AudioProcessorGraph::UpdateKind::none);

    if (inputNode == nullptr || trackMixerNode == nullptr || gainNode == nullptr || outputNode == nullptr)
        return;

    const auto inputId      = inputNode->nodeID;
    const auto trackMixerId = trackMixerNode->nodeID;
    const auto gainId       = gainNode->nodeID;
    const auto outputId     = outputNode->nodeID;

    for (int ch = 0; ch < TrackMixerProcessor::maxInputChannels; ++ch)
        connectChannel (graph, inputId, trackMixerId, ch);

    if (pluginNode != nullptr)
    {
        const auto pluginId = pluginNode->nodeID;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            connectChannel (graph, trackMixerId, pluginId, ch);
            connectChannel (graph, pluginId, gainId, ch);
            connectChannel (graph, gainId, outputId, ch);
        }
    }
    else
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            connectChannel (graph, trackMixerId, gainId, ch);
            connectChannel (graph, gainId, outputId, ch);
        }
    }

    graph.rebuild();
}

void PluginChain::scanFormat (juce::AudioPluginFormat& format, const juce::FileSearchPath& searchPaths)
{
    if (searchPaths.getNumPaths() == 0)
        return;

    juce::PluginDirectoryScanner scanner (knownPluginList,
                                          format,
                                          searchPaths,
                                          true,
                                          deadMansPedalFile,
                                          true);

    juce::String pluginName;

    while (scanner.scanNextFile (true, pluginName))
    {
    }

    if (format.getName() == "VST3")
        scanWaveShellFiles (knownPluginList, format, "WaveShell*.vst3", searchPaths);
    else if (format.getName() == "VST")
        scanWaveShellFiles (knownPluginList, format, "WaveShell*.dll", searchPaths);
}

void PluginChain::scanPlugins()
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    knownPluginList.clear();

    if (formatManager == nullptr)
        return;

    if (! scanPaths.hasAnyScannablePaths())
    {
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                    "Plugin Scan",
                                                    juce::String::fromUTF8 (
                                                        u8"VST / VST2 / VST3 \u306e\u30b9\u30ad\u30e3\u30f3\u5bfe\u8c61\u30c7\u30a3\u30ec\u30c8\u30ea\u304c\u3042\u308a\u307e\u305b\u3093\u3002"
                                                        u8"\u5404\u30bf\u30d6\u3067\u30d5\u30a9\u30eb\u30c0\u3092\u8ffd\u52a0\u3057\u3066\u304f\u3060\u3055\u3044\u3002"));
        return;
    }

    for (int i = 0; i < formatManager->getNumFormats(); ++i)
    {
        if (auto* format = formatManager->getFormat (i))
        {
            const auto name = format->getName();

            if (name == "VST3")
            {
                const auto vst3Paths = scanPaths.getSearchPath (PluginPathCategory::Vst3);

                if (vst3Paths.getNumPaths() > 0)
                    scanFormat (*format, vst3Paths);
            }
            else if (name == "VST")
            {
                const auto vstPaths = scanPaths.getVst2CombinedSearchPath();

                if (vstPaths.getNumPaths() > 0)
                    scanFormat (*format, vstPaths);
            }
        }
    }

    sendChangeMessage();
}

juce::Array<juce::PluginDescription> PluginChain::getPluginsForUi() const
{
    juce::Array<juce::PluginDescription> plugins;

    for (const auto& type : knownPluginList.getTypes())
        if (isUiSelectablePlugin (type))
            plugins.add (type);

    struct PluginNameComparator
    {
        static int compareElements (const juce::PluginDescription& a, const juce::PluginDescription& b)
        {
            const auto nameA = a.name + a.pluginFormatName;
            const auto nameB = b.name + b.pluginFormatName;
            return nameA.compareIgnoreCase (nameB);
        }
    };

    plugins.sort (PluginNameComparator());

    return plugins;
}

void PluginChain::loadPlugin (const juce::PluginDescription& description)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (isWavesShellGateway (description))
    {
        showPluginLoadError ({}, description);
        return;
    }

    const auto sampleRate = graph.getSampleRate() > 0 ? graph.getSampleRate() : 44100.0;
    const auto blockSize  = graph.getBlockSize() > 0 ? graph.getBlockSize() : 512;

    juce::String error;
    auto instance = formatManager->createPluginInstance (description, sampleRate, blockSize, error);

    if (instance == nullptr)
    {
        showPluginLoadError (error, description);
        return;
    }

    setPluginInstance (std::move (instance));
}

void PluginChain::loadPluginFromFile (const juce::File& pluginFile)
{
    if (formatManager == nullptr)
        return;

    for (int i = 0; i < formatManager->getNumFormats(); ++i)
    {
        if (auto* format = formatManager->getFormat (i))
        {
            const auto name = format->getName();

            if (name != "VST3" && name != "VST")
                continue;

            juce::OwnedArray<juce::PluginDescription> typesFound;
            knownPluginList.scanAndAddFile (pluginFile.getFullPathName(), true, typesFound, *format);

            if (const auto* preferred = pickPreferredPlugin (typesFound))
            {
                loadPlugin (*preferred);
                return;
            }
        }
    }

    juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Plugin Load Failed",
                                                "Could not load: " + pluginFile.getFullPathName());
}

void PluginChain::setPluginInstance (std::unique_ptr<juce::AudioPluginInstance> instance)
{
    hidePluginEditor();

    if (pluginNode != nullptr)
        graph.removeNode (pluginNode);

    instance->enableAllBuses();
    pluginNode = graph.addNode (std::move (instance));
    rebuildConnections();
    sendChangeMessage();
}

void PluginChain::clearPlugin()
{
    hidePluginEditor();

    if (pluginNode != nullptr)
    {
        if (auto* processor = pluginNode->getProcessor())
            processor->releaseResources();

        graph.removeNode (pluginNode);
        pluginNode = nullptr;
        rebuildConnections();
        sendChangeMessage();
    }
}

void PluginChain::releaseAll()
{
    hidePluginEditor();
    clearPlugin();

    knownPluginList.clear();
    formatManager = std::make_unique<juce::AudioPluginFormatManager>();
    juce::addDefaultFormatsToManager (*formatManager);

    sendChangeMessage();
}

juce::String PluginChain::getLoadedPluginName() const
{
    if (pluginNode == nullptr)
        return "None";

    if (auto* processor = pluginNode->getProcessor())
        return processor->getName();

    return "None";
}

void PluginChain::setGain (float newGain)
{
    if (gainProcessor != nullptr)
        gainProcessor->gain.store (newGain);
}

void PluginChain::showPluginEditor()
{
    if (pluginNode == nullptr)
        return;

    auto* processor = pluginNode->getProcessor();

    if (processor == nullptr || ! processor->hasEditor())
        return;

    if (pluginEditorWindow != nullptr)
    {
        pluginEditorWindow->toFront (true);
        return;
    }

    if (auto* editor = processor->createEditorIfNeeded())
    {
        struct PluginWindow final : public juce::DocumentWindow
        {
            PluginWindow (const juce::String& name, juce::AudioProcessorEditor& ed)
                : juce::DocumentWindow (name,
                                        juce::Desktop::getInstance().getDefaultLookAndFeel()
                                            .findColour (juce::ResizableWindow::backgroundColourId),
                                        juce::DocumentWindow::allButtons)
            {
                setUsingNativeTitleBar (true);
                setContentNonOwned (&ed, true);
                centreWithSize (ed.getWidth(), ed.getHeight());
                setResizable (ed.isResizable(), false);
                setVisible (true);
            }

            void closeButtonPressed() override { setVisible (false); }
        };

        pluginEditorWindow = std::make_unique<PluginWindow> (processor->getName(), *editor);
    }
}

void PluginChain::hidePluginEditor()
{
    if (auto* processor = pluginNode != nullptr ? pluginNode->getProcessor() : nullptr)
        if (auto* editor = processor->getActiveEditor())
            processor->editorBeingDeleted (editor);

    pluginEditorWindow = nullptr;
}

#include "PluginChain.h"
#include "DrizzleAudioProcessor.h"
#include "Vst3HostIdentity.h"

namespace
{
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

int getVst3PathPreferenceRank (const juce::String& path)
{
    const auto normalised = path.replaceCharacter ('/', '\\').toLowerCase();

    if (normalised.contains ("\\common files\\vst3\\"))
        return 0;

    if (normalised.contains ("\\programs\\common\\vst3\\"))
        return 1;

    if (normalised.contains ("\\vstplugins\\")
        || normalised.contains ("\\steinberg\\vstplugins\\"))
        return 100;

    return 50;
}

bool isNonCanonicalVst3Path (const juce::PluginDescription& description)
{
    return description.pluginFormatName == "VST3"
        && getVst3PathPreferenceRank (description.fileOrIdentifier) >= 50;
}

juce::PluginDescription preferCanonicalPluginDescription (const juce::PluginDescription& description,
                                                        const juce::KnownPluginList& knownPluginList)
{
    if (description.pluginFormatName != "VST3")
        return description;

    auto best = description;
    auto bestRank = getVst3PathPreferenceRank (best.fileOrIdentifier);

    for (const auto& type : knownPluginList.getTypes())
    {
        if (type.uniqueId != description.uniqueId || type.pluginFormatName != "VST3")
            continue;

        const auto rank = getVst3PathPreferenceRank (type.fileOrIdentifier);

        if (rank < bestRank)
        {
            best = type;
            bestRank = rank;
        }
    }

    return best;
}

juce::Array<juce::PluginDescription> deduplicatePluginsForUi (const juce::KnownPluginList& knownPluginList)
{
    juce::Array<juce::PluginDescription> plugins;

    for (const auto& type : knownPluginList.getTypes())
    {
        if (! isUiSelectablePlugin (type))
            continue;

        bool replaced = false;

        for (int i = 0; i < plugins.size(); ++i)
        {
            if (plugins.getReference (i).uniqueId != type.uniqueId)
                continue;

            const auto existingRank = getVst3PathPreferenceRank (plugins.getReference (i).fileOrIdentifier);
            const auto candidateRank = getVst3PathPreferenceRank (type.fileOrIdentifier);

            if (candidateRank < existingRank)
                plugins.getReference (i) = type;

            replaced = true;
            break;
        }

        if (! replaced)
            plugins.add (type);
    }

    return plugins;
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
             || description.name.containsIgnoreCase ("Waves"))
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
    else if (error.containsIgnoreCase ("license")
             || error.containsIgnoreCase ("licence")
             || isNonCanonicalVst3Path (description))
    {
        message << juce::String::fromUTF8 (
            "\n\n[\u30e9\u30a4\u30bb\u30f3\u30b9\u691c\u8a3c\u306e\u30d2\u30f3\u30c8]\n"
            "VST3 \u306f\u6b21\u306e\u6b63\u898f\u30d1\u30b9\u304b\u3089\u8aad\u307f\u8fbc\u3093\u3067\u304f\u3060\u3055\u3044:\n"
            "  C:\\Program Files\\Common Files\\VST3\n"
            "VstPlugIns \u7b49\u306e\u30b3\u30d4\u30fc\u3084\u30ab\u30b9\u30bf\u30e0\u30d5\u30a9\u30eb\u30c0\u304b\u3089\u306e\u8aad\u307f\u8fbc\u307f\u3067\u3001"
            "\u8a8d\u8a3c\u6e08\u307f\u3067\u3082\u300c\u30e9\u30a4\u30bb\u30f3\u30b9\u304c\u898b\u3064\u304b\u3089\u306a\u3044\u300d\u3068\u51fa\u308b\u3053\u3068\u304c\u3042\u308a\u307e\u3059\u3002\n"
            "1. \u8a2d\u5b9a \u2192 VST \u3067 VST3 \u30bf\u30d6\u306b Common Files\\VST3 \u304c\u542b\u307e\u308c\u3066\u3044\u308b\u304b\u78ba\u8a8d\n"
            "2. \u518d\u30b9\u30ad\u30e3\u30f3\u5f8c\u3001\u30c8\u30e9\u30c3\u30af VST \u306e\u300cVST3\u300d\u30bf\u30d6\u304b\u3089\u9078\u629e\n"
            "3. \u30e1\u30fc\u30ab\u30fc\u306e\u30a4\u30f3\u30b9\u30g\u30fc\u30e9\u3067\u30e9\u30a4\u30bb\u30f3\u30b9\u540c\u671f\u3092\u78ba\u8a8d\n"
            "   (XLN: XLN Online Installer / Waves: Waves Central \u306a\u3069)\n\n")
            << "Path: " << description.fileOrIdentifier << "\n\n"
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
} // namespace

PluginChain::PluginChain()
    : deadMansPedalFile (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                             .getChildFile ("Drizzle")
                             .getChildFile ("dead_mans_pedal.txt"))
{
    audioProcessor = std::make_unique<DrizzleAudioProcessor>();
    formatManager = std::make_unique<juce::AudioPluginFormatManager>();
    juce::addDefaultFormatsToManager (*formatManager);
    deadMansPedalFile.getParentDirectory().createDirectory();

    trackPluginManager.setFormatManager (formatManager.get());
    audioProcessor->setTrackPluginManager (&trackPluginManager);
}

PluginChain::~PluginChain()
{
    releaseAll();
}

juce::AudioProcessor& PluginChain::getAudioProcessor() noexcept
{
    return *audioProcessor;
}

TrackMixerProcessor& PluginChain::getTrackMixer() noexcept
{
    return audioProcessor->getTrackMixer();
}

const TrackMixerProcessor& PluginChain::getTrackMixer() const noexcept
{
    return audioProcessor->getTrackMixer();
}

void PluginChain::handleAudioDeviceChanged (int activeInputChannels)
{
    if (audioProcessor != nullptr && activeInputChannels > 0)
        audioProcessor->setConnectedInputChannelCount (activeInputChannels);
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

    const juce::FileSearchPath vst3SearchPaths = scanPaths.getSearchPath (PluginPathCategory::Vst3);

    for (int i = 0; i < formatManager->getNumFormats(); ++i)
    {
        if (auto* format = formatManager->getFormat (i))
        {
            const auto name = format->getName();

            if (name == "VST3")
            {
                if (vst3SearchPaths.getNumPaths() > 0)
                    scanFormat (*format, vst3SearchPaths);
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
    auto plugins = deduplicatePluginsForUi (knownPluginList);

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

    const auto resolved = preferCanonicalPluginDescription (description, knownPluginList);
    const auto sampleRate = audioProcessor->getSampleRate() > 0 ? audioProcessor->getSampleRate() : 44100.0;
    const auto blockSize  = audioProcessor->getBlockSize() > 0 ? audioProcessor->getBlockSize() : 512;

    DrizzleVst3Host::prepareForLicensedPluginLoad (resolved.manufacturerName);

    juce::String error;
    auto instance = formatManager->createPluginInstance (resolved, sampleRate, blockSize, error);

    if (instance == nullptr)
    {
        showPluginLoadError (error, resolved);
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
    audioProcessor->setPluginInstance (std::move (instance));
    sendChangeMessage();
}

void PluginChain::clearPlugin()
{
    hidePluginEditor();
    audioProcessor->setPluginInstance (nullptr);
    sendChangeMessage();
}

void PluginChain::unloadMasterPlugin()
{
    clearPlugin();
}

bool PluginChain::hasScannedPlugins() const noexcept
{
    return knownPluginList.getNumTypes() > 0;
}

bool PluginChain::isVst2HostingEnabled() noexcept
{
#if DRIZZLE_HAS_VST2
    return true;
#else
    return false;
#endif
}

void PluginChain::releaseAll()
{
    hidePluginEditor();
    clearPlugin();
    trackPluginManager.clearAll();

    knownPluginList.clear();
    formatManager = std::make_unique<juce::AudioPluginFormatManager>();
    juce::addDefaultFormatsToManager (*formatManager);
    trackPluginManager.setFormatManager (formatManager.get());

    sendChangeMessage();
}

juce::Array<juce::PluginDescription> PluginChain::getPluginsForUiByFormat (const juce::String& formatName) const
{
    juce::Array<juce::PluginDescription> plugins;

    for (const auto& plugin : getPluginsForUi())
    {
        if (formatName.isEmpty() || plugin.pluginFormatName.equalsIgnoreCase (formatName))
            plugins.add (plugin);
    }

    return plugins;
}

bool PluginChain::addTrackPlugin (int trackIndex, const juce::PluginDescription& description)
{
    const auto resolved = preferCanonicalPluginDescription (description, knownPluginList);
    juce::String error;

    if (trackPluginManager.addPlugin (trackIndex, resolved, error))
        return true;

    if (error.isNotEmpty())
        showPluginLoadError (error, resolved);

    return false;
}

bool PluginChain::removeTrackPlugin (int trackIndex, int slotIndex)
{
    return trackPluginManager.removePlugin (trackIndex, slotIndex);
}

int PluginChain::getNumTrackPlugins (int trackIndex) const
{
    return trackPluginManager.getNumPlugins (trackIndex);
}

juce::String PluginChain::getTrackPluginName (int trackIndex, int slotIndex) const
{
    return trackPluginManager.getPluginName (trackIndex, slotIndex);
}

void PluginChain::showTrackPluginEditor (int trackIndex, int slotIndex, juce::Component* modalParent)
{
    trackPluginManager.showPluginEditor (trackIndex, slotIndex, modalParent);
}

void PluginChain::onTrackRemoved (int removedIndex, int numTracksAfterRemove)
{
    trackPluginManager.shiftPluginsOnTrackRemove (removedIndex, numTracksAfterRemove);
}

bool PluginChain::hasPluginLoaded() const noexcept
{
    return audioProcessor != nullptr && audioProcessor->hasPluginLoaded();
}

juce::String PluginChain::getLoadedPluginName() const
{
    if (auto* plugin = audioProcessor != nullptr ? audioProcessor->getPluginInstance() : nullptr)
        return plugin->getName();

    return "None";
}

void PluginChain::setGain (float newGain)
{
    if (audioProcessor != nullptr)
        audioProcessor->setPluginGain (newGain);
}

void PluginChain::setMasterGainDb (float gainDb)
{
    if (audioProcessor != nullptr)
        audioProcessor->setMasterGainDb (gainDb);
}

void PluginChain::setMasterMute (bool mute)
{
    if (audioProcessor != nullptr)
        audioProcessor->setMasterMute (mute);
}

void PluginChain::setMasterMono (bool mono)
{
    if (audioProcessor != nullptr)
        audioProcessor->setMasterMono (mono);
}

float PluginChain::getMasterPeakLevel() const noexcept
{
    if (audioProcessor != nullptr)
        return audioProcessor->getMasterPeakLevel();

    return 0.0f;
}

void PluginChain::showPluginEditor()
{
    auto* processor = audioProcessor != nullptr ? audioProcessor->getPluginInstance() : nullptr;

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
    if (auto* processor = audioProcessor != nullptr ? audioProcessor->getPluginInstance() : nullptr)
        if (auto* editor = processor->getActiveEditor())
            processor->editorBeingDeleted (editor);

    pluginEditorWindow = nullptr;
}

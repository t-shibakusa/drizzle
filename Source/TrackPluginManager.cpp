#include "TrackPluginManager.h"
#include "Vst3HostIdentity.h"

namespace
{
bool isValidTrackIndex (int trackIndex) noexcept
{
    return juce::isPositiveAndBelow (trackIndex, TrackMixerProcessor::maxTracks);
}

bool isValidSlotIndex (int trackIndex, int slotIndex, const std::vector<TrackPluginManager::PluginSlot>& slots) noexcept
{
    return isValidTrackIndex (trackIndex) && juce::isPositiveAndBelow (slotIndex, (int) slots.size());
}

juce::DialogWindow* findHostingDialog (juce::Component* component)
{
    if (component == nullptr)
        return nullptr;

    if (auto* dialog = dynamic_cast<juce::DialogWindow*> (component))
        return dialog;

    return component->findParentComponentOfClass<juce::DialogWindow>();
}

class TrackPluginEditorWindow final : public juce::DocumentWindow
{
public:
    TrackPluginEditorWindow (const juce::String& name,
                             juce::AudioProcessorEditor& editor,
                             juce::Component* modalParentIn,
                             std::function<void()> onCloseIn)
        : juce::DocumentWindow (name,
                                  juce::Desktop::getInstance().getDefaultLookAndFeel()
                                      .findColour (juce::ResizableWindow::backgroundColourId),
                                  juce::DocumentWindow::allButtons),
          hostDialog (findHostingDialog (modalParentIn)),
          onClose (std::move (onCloseIn))
    {
        setUsingNativeTitleBar (true);
        setContentNonOwned (&editor, true);
        setResizable (editor.isResizable(), false);
        setAlwaysOnTop (true);

        if (hostDialog != nullptr && hostDialog->isCurrentlyModal())
        {
            hostDialog->exitModalState (0);
            suspendedHostModal = true;
        }

        if (hostDialog != nullptr)
            centreAroundComponent (hostDialog.getComponent(), editor.getWidth(), editor.getHeight());
        else if (modalParentIn != nullptr)
            centreAroundComponent (modalParentIn, editor.getWidth(), editor.getHeight());
        else
            centreWithSize (editor.getWidth(), editor.getHeight());

        setVisible (true);
        enterModalState (true, nullptr, false);
        toFront (true);
    }

    ~TrackPluginEditorWindow() override
    {
        if (isCurrentlyModal())
            exitModalState (0);

        resumeHostDialogModal();
    }

    void focusEditor()
    {
        setAlwaysOnTop (true);

        if (hostDialog != nullptr && hostDialog->isCurrentlyModal())
        {
            hostDialog->exitModalState (0);
            suspendedHostModal = true;
        }

        if (! isCurrentlyModal())
            enterModalState (true, nullptr, false);

        toFront (true);
    }

    void closeButtonPressed() override
    {
        exitModalState (0);

        if (onClose != nullptr)
            onClose();
    }

private:
    void resumeHostDialogModal()
    {
        if (! suspendedHostModal)
            return;

        suspendedHostModal = false;

        if (hostDialog != nullptr && hostDialog->isVisible())
            hostDialog->enterModalState (true, nullptr, false);
    }

    juce::Component::SafePointer<juce::DialogWindow> hostDialog;
    bool suspendedHostModal = false;
    std::function<void()> onClose;
};
} // namespace

TrackPluginManager::TrackPluginManager() = default;

void TrackPluginManager::setFormatManager (juce::AudioPluginFormatManager* manager) noexcept
{
    formatManager = manager;
}

void TrackPluginManager::prepareToPlay (double sampleRate, int blockSize)
{
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;

    const juce::ScopedLock sl (lock);

    for (auto& slots : trackPlugins)
    {
        for (auto& slot : slots)
        {
            if (slot.instance != nullptr)
                slot.instance->prepareToPlay (sampleRate, blockSize);
        }
    }
}

void TrackPluginManager::releaseResources()
{
    hideAllEditors();

    const juce::ScopedLock sl (lock);

    for (auto& slots : trackPlugins)
    {
        for (auto& slot : slots)
        {
            if (slot.instance != nullptr)
                slot.instance->releaseResources();
        }
    }
}

int TrackPluginManager::getNumPlugins (int trackIndex) const
{
    if (! isValidTrackIndex (trackIndex))
        return 0;

    const juce::ScopedLock sl (lock);
    return (int) trackPlugins[(size_t) trackIndex].size();
}

juce::String TrackPluginManager::getPluginName (int trackIndex, int slotIndex) const
{
    if (! isValidTrackIndex (trackIndex))
        return {};

    const juce::ScopedLock sl (lock);
    const auto& slots = trackPlugins[(size_t) trackIndex];

    if (! juce::isPositiveAndBelow (slotIndex, (int) slots.size()))
        return {};

    return slots[(size_t) slotIndex].description.name;
}

juce::Array<juce::PluginDescription> TrackPluginManager::getPluginDescriptions (int trackIndex) const
{
    juce::Array<juce::PluginDescription> result;

    if (! isValidTrackIndex (trackIndex))
        return result;

    const juce::ScopedLock sl (lock);

    for (const auto& slot : trackPlugins[(size_t) trackIndex])
        result.add (slot.description);

    return result;
}

std::unique_ptr<juce::AudioPluginInstance> TrackPluginManager::createInstance (const juce::PluginDescription& description,
                                                                                juce::String& errorOut) const
{
    if (formatManager == nullptr)
    {
        errorOut = "Plugin format manager is not available.";
        return nullptr;
    }

    DrizzleVst3Host::prepareForLicensedPluginLoad (description.manufacturerName);
    return formatManager->createPluginInstance (description, currentSampleRate, currentBlockSize, errorOut);
}

bool TrackPluginManager::addPlugin (int trackIndex, const juce::PluginDescription& description, juce::String& errorOut)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (! isValidTrackIndex (trackIndex))
        return false;

    auto instance = createInstance (description, errorOut);

    if (instance == nullptr)
        return false;

    instance->enableAllBuses();

    if (currentSampleRate > 0.0 && currentBlockSize > 0)
        instance->prepareToPlay (currentSampleRate, currentBlockSize);

    {
        const juce::ScopedLock sl (lock);
        auto& slots = trackPlugins[(size_t) trackIndex];

        if (slots.size() >= (size_t) maxPluginsPerTrack)
        {
            errorOut = juce::String::fromUTF8 (u8"\u30c8\u30e9\u30c3\u30af\u306b\u8ffd\u52a0\u3067\u304d\u308b\u30d7\u30e9\u30b0\u30a4\u30f3\u306e\u4e0a\u9650\u306b\u9054\u3057\u307e\u3057\u305f\u3002");
            return false;
        }

        PluginSlot slot;
        slot.description = description;
        slot.instance = std::move (instance);
        slots.push_back (std::move (slot));
    }

    sendChangeMessage();
    return true;
}

bool TrackPluginManager::removePlugin (int trackIndex, int slotIndex)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (! isValidTrackIndex (trackIndex))
        return false;

    hidePluginEditor (trackIndex, slotIndex);

    const juce::ScopedLock sl (lock);
    auto& slots = trackPlugins[(size_t) trackIndex];

    if (! juce::isPositiveAndBelow (slotIndex, (int) slots.size()))
        return false;

    if (slots[(size_t) slotIndex].instance != nullptr)
        slots[(size_t) slotIndex].instance->releaseResources();

    slots.erase (slots.begin() + slotIndex);

    sendChangeMessage();
    return true;
}

void TrackPluginManager::clearTrack (int trackIndex)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (! isValidTrackIndex (trackIndex))
        return;

    for (int i = getNumPlugins (trackIndex) - 1; i >= 0; --i)
        hidePluginEditor (trackIndex, i);

    const juce::ScopedLock sl (lock);
    auto& slots = trackPlugins[(size_t) trackIndex];

    for (auto& slot : slots)
    {
        if (slot.instance != nullptr)
            slot.instance->releaseResources();
    }

    slots.clear();
    sendChangeMessage();
}

void TrackPluginManager::shiftPluginsOnTrackRemove (int removedIndex, int numTracksAfterRemove)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (! isValidTrackIndex (removedIndex))
        return;

    hideAllEditors();

    const juce::ScopedLock sl (lock);

    for (auto& slot : trackPlugins[(size_t) removedIndex])
    {
        if (slot.instance != nullptr)
            slot.instance->releaseResources();
    }

    trackPlugins[(size_t) removedIndex].clear();

    for (int i = removedIndex; i < numTracksAfterRemove; ++i)
        trackPlugins[(size_t) i] = std::move (trackPlugins[(size_t) (i + 1)]);

    trackPlugins[(size_t) numTracksAfterRemove].clear();
    sendChangeMessage();
}

void TrackPluginManager::clearAll()
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    hideAllEditors();

    const juce::ScopedLock sl (lock);

    for (auto& slots : trackPlugins)
    {
        for (auto& slot : slots)
        {
            if (slot.instance != nullptr)
                slot.instance->releaseResources();
        }

        slots.clear();
    }

    sendChangeMessage();
}

void TrackPluginManager::processTrack (int trackIndex, juce::AudioBuffer<float>& stereoBuffer, juce::MidiBuffer& midi)
{
    if (! isValidTrackIndex (trackIndex))
        return;

    const juce::ScopedLock sl (lock);

    for (auto& slot : trackPlugins[(size_t) trackIndex])
    {
        if (slot.instance == nullptr)
            continue;

        slot.instance->processBlock (stereoBuffer, midi);
    }
}

void TrackPluginManager::showPluginEditor (int trackIndex, int slotIndex, juce::Component* modalParent)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (! isValidTrackIndex (trackIndex))
        return;

    juce::AudioPluginInstance* processor = nullptr;
    juce::String pluginName;

    {
        const juce::ScopedLock sl (lock);
        const auto& slots = trackPlugins[(size_t) trackIndex];

        if (! juce::isPositiveAndBelow (slotIndex, (int) slots.size()))
            return;

        processor = slots[(size_t) slotIndex].instance.get();
        pluginName = slots[(size_t) slotIndex].description.name;
    }

    if (processor == nullptr || ! processor->hasEditor())
        return;

    const EditorKey key { trackIndex, slotIndex };

    if (auto it = editorWindows.find (key); it != editorWindows.end() && it->second != nullptr)
    {
        if (auto* existing = dynamic_cast<TrackPluginEditorWindow*> (it->second.get()))
            existing->focusEditor();

        return;
    }

    if (auto* editor = processor->createEditorIfNeeded())
    {
        editorWindows[key] = std::make_unique<TrackPluginEditorWindow> (pluginName,
                                                                        *editor,
                                                                        modalParent,
                                                                        [this, key]
                                                                        {
                                                                            hidePluginEditor (key.trackIndex, key.slotIndex);
                                                                        });
    }
}

void TrackPluginManager::hidePluginEditor (int trackIndex, int slotIndex)
{
    const EditorKey key { trackIndex, slotIndex };
    auto it = editorWindows.find (key);

    if (it == editorWindows.end())
        return;

    juce::AudioPluginInstance* processor = nullptr;

    {
        const juce::ScopedLock sl (lock);
        const auto& slots = trackPlugins[(size_t) trackIndex];

        if (juce::isPositiveAndBelow (slotIndex, (int) slots.size()))
            processor = slots[(size_t) slotIndex].instance.get();
    }

    if (processor != nullptr)
        if (auto* editor = processor->getActiveEditor())
            processor->editorBeingDeleted (editor);

    editorWindows.erase (it);
}

void TrackPluginManager::hideAllEditors()
{
    std::vector<EditorKey> keys;

    for (const auto& entry : editorWindows)
        keys.push_back (entry.first);

    for (const auto& key : keys)
        hidePluginEditor (key.trackIndex, key.slotIndex);
}

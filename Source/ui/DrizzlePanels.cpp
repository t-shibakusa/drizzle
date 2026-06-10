#include "DrizzlePanels.h"
#include "TrackPluginPanel.h"
#include "MixerDbScale.h"
#include "../Vst3HostIdentity.h"
#include <functional>

namespace
{
constexpr int kFaderMeterWidth = 10;

void paintHorizontalMeter (juce::Graphics& g, juce::Rectangle<int> area, float level)
{
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (area.toFloat(), 3.0f);

    const auto filled = area.withWidth (juce::roundToInt ((float) area.getWidth() * juce::jlimit (0.0f, 1.0f, level)));
    juce::ColourGradient grad (DrizzleTheme::meterGreen(), 0, 0,
                               DrizzleTheme::meterRed(), (float) area.getRight(), 0, false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (filled.toFloat(), 3.0f);
}

void paintVerticalMeter (juce::Graphics& g, juce::Rectangle<int> area, float level)
{
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (area.toFloat(), 2.0f);

    auto filled = area.removeFromBottom (juce::roundToInt ((float) area.getHeight() * juce::jlimit (0.0f, 1.0f, level)));
    juce::ColourGradient grad (DrizzleTheme::meterGreen(), 0, (float) area.getBottom(),
                               DrizzleTheme::meterRed(), 0, 0, false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (filled.toFloat(), 2.0f);
}

juce::String formatLiveDuration (double seconds)
{
    const int total = juce::jmax (0, (int) seconds);
    const int hours = total / 3600;
    const int minutes = (total % 3600) / 60;
    const int secs = total % 60;
    return juce::String::formatted ("%02d:%02d:%02d", hours, minutes, secs);
}
} // namespace

namespace
{
class InputSelectionPanel final : public juce::Component,
                                  private juce::ListBoxModel
{
public:
    InputSelectionPanel (AudioEngine& engine, int trackIndexIn, std::function<void()> onSelected)
        : audioEngine (engine),
          trackIndex (trackIndexIn),
          onSelectedCallback (std::move (onSelected))
    {
        titleLabel.setText (juce::String::fromUTF8 (u8"\u5165\u529b\u9078\u629e"),
                            juce::dontSendNotification);
        DrizzleTheme::applyLabel (titleLabel);

        options = audioEngine.getAvailableInputOptions();
        list.setModel (this);
        list.setRowHeight (24);
        list.selectRow (findRowForChannel (audioEngine.getTrackInputChannel (trackIndex)));

        okButton.setButtonText (juce::String::fromUTF8 (u8"\u9078\u629e"));
        cancelButton.setButtonText (juce::String::fromUTF8 (u8"\u30ad\u30e3\u30f3\u30bb\u30eb"));

        okButton.onClick = [this] { applySelection(); };
        cancelButton.onClick = [this]
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState (0);
        };

        addAndMakeVisible (titleLabel);
        addAndMakeVisible (list);
        addAndMakeVisible (okButton);
        addAndMakeVisible (cancelButton);

        setSize (420, 320);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        titleLabel.setBounds (area.removeFromTop (24));
        area.removeFromTop (8);
        auto buttons = area.removeFromBottom (32);
        okButton.setBounds (buttons.removeFromRight (90).reduced (2));
        cancelButton.setBounds (buttons.removeFromRight (90).reduced (2));
        list.setBounds (area);
    }

private:
    int getNumRows() override { return options.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, options.size()))
            return;

        if (selected)
            g.fillAll (DrizzleTheme::accent().withAlpha (0.35f));

        g.setColour (DrizzleTheme::textPrimary());
        g.setFont (juce::FontOptions { 13.0f });
        g.drawText (options.getReference (row).label, 6, 0, width - 12, height, juce::Justification::centredLeft);
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
    {
        list.selectRow (row);
        applySelection();
    }

    int findRowForChannel (int channel) const
    {
        for (int i = 0; i < options.size(); ++i)
            if (options.getReference (i).channelIndex == channel)
                return i;

        return 0;
    }

    void applySelection()
    {
        const int row = list.getSelectedRow();

        if (juce::isPositiveAndBelow (row, options.size()))
            audioEngine.setTrackInputChannel (trackIndex, options.getReference (row).channelIndex);

        audioEngine.saveSessionSettings();

        if (onSelectedCallback)
            onSelectedCallback();

        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (1);
    }

    AudioEngine& audioEngine;
    int trackIndex;
    juce::Array<AudioInputOption> options;
    juce::Label titleLabel;
    juce::ListBox list { "Inputs", this };
    juce::TextButton okButton;
    juce::TextButton cancelButton;
    std::function<void()> onSelectedCallback;
};
} // namespace

namespace
{
constexpr int kTrackRowContentHeight = 110;
constexpr int kTrackRowGap           = 8;
constexpr int kTrackRowHeight        = kTrackRowContentHeight + kTrackRowGap;
constexpr int kAddTrackButtonHeight = 52;
constexpr int kMasterSectionHeight  = 130;
constexpr int kPanelTitleHeight     = 28;
constexpr int kFaderWidth           = 76;
constexpr int kFaderMeterGap        = 12;
} // namespace

//==============================================================================
class TrackColourSwatchButton final : public juce::Component
{
public:
    std::function<void()> onClick;

    void setColour (juce::Colour newColour)
    {
        swatchColour = newColour;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

        g.setColour (swatchColour);
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (onClick != nullptr)
            onClick();
    }

private:
    juce::Colour swatchColour { DrizzleTheme::accent() };
};

class TrackColourPickerContent final : public juce::Component,
                                       private juce::ChangeListener
{
public:
    TrackColourPickerContent (juce::Colour initialColour,
                              std::function<void (juce::Colour)> onColourChangedIn)
        : onColourChanged (std::move (onColourChangedIn))
    {
        colourSelector.setCurrentColour (initialColour);
        colourSelector.addChangeListener (this);
        addAndMakeVisible (colourSelector);
        setSize (300, 320);
    }

    void resized() override
    {
        colourSelector.setBounds (getLocalBounds());
    }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        if (onColourChanged != nullptr)
            onColourChanged (colourSelector.getCurrentColour());
    }

    juce::ColourSelector colourSelector;
    std::function<void (juce::Colour)> onColourChanged;
};

class TrackMixerPanel::TracksListContent final : public juce::Component
{
public:
    int getContentHeight() const noexcept
    {
        return juce::jmax (1, owner.getNumTrackRows()) * kTrackRowHeight;
    }

    void resized() override
    {
        owner.layoutTracksList();
    }

    explicit TracksListContent (TrackMixerPanel& panelIn)
        : owner (panelIn)
    {
    }

private:
    TrackMixerPanel& owner;
};

class TrackMixerPanel::TrackRowComponent final : public juce::Component
{
public:
    bool isInteractiveHit (juce::Point<int> pos) const
    {
        const juce::Component* interactive[] {
            &removeButton, &inputButton, &soloButton, &muteButton,
            &panSlider, &faderSlider, &colourButton, &nameLabel
        };

        for (const auto* component : interactive)
        {
            if (component->isVisible() && component->getBounds().contains (pos))
                return true;
        }

        return false;
    }

    TrackRowComponent (TrackMixerPanel& panelIn,
                       AudioEngine& engineIn,
                       MixerFaderLookAndFeel& lafIn,
                       int index)
        : panel (panelIn),
          audioEngine (engineIn),
          faderLookAndFeel (lafIn),
          trackIndex (index)
    {
        nameLabel.setJustificationType (juce::Justification::centredLeft);
        nameLabel.setEditable (true, false, false);
        DrizzleTheme::applyLabel (nameLabel, true);
        nameLabel.setFont (juce::FontOptions { 14.0f }.withStyle ("Bold"));
        nameLabel.setColour (juce::Label::textWhenEditingColourId, DrizzleTheme::textPrimary());
        nameLabel.setColour (juce::Label::backgroundWhenEditingColourId, juce::Colours::transparentBlack);
        nameLabel.setColour (juce::Label::outlineWhenEditingColourId, juce::Colours::transparentBlack);
        nameLabel.onEditorHide = [this]
        {
            juce::String text = nameLabel.getText();

            if (auto* editor = nameLabel.getCurrentTextEditor())
                text = editor->getText();

            audioEngine.setTrackName (trackIndex, text);
            nameLabel.setText (audioEngine.getTrackName (trackIndex), juce::dontSendNotification);
        };

        removeButton.setButtonText (juce::String::fromUTF8 (u8"\u00d7"));
        removeButton.setTooltip (juce::String::fromUTF8 (u8"\u30c8\u30e9\u30c3\u30af\u3092\u524a\u9664"));
        removeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        removeButton.setColour (juce::TextButton::textColourOffId, DrizzleTheme::textMuted());
        removeButton.onClick = [this] { panel.confirmRemoveTrack (trackIndex); };

        colourButton.onClick = [this] { panel.showTrackColourPicker (trackIndex, colourButton); };

        inputButton.setClickingTogglesState (false);
        inputButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        inputButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        inputButton.setColour (juce::TextButton::textColourOffId, DrizzleTheme::textMuted());
        inputButton.onClick = [this] { panel.showInputSelectionForTrack (trackIndex); };

        fxAreaButton.setButtonText ("VST");
        fxAreaButton.setTooltip (juce::String::fromUTF8 (u8"VST \u3092\u8ffd\u52a0\u30fb\u7de8\u96c6"));
        fxAreaButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        fxAreaButton.setColour (juce::TextButton::buttonOnColourId, DrizzleTheme::accent().withAlpha (0.15f));
        fxAreaButton.setColour (juce::TextButton::textColourOffId, DrizzleTheme::textMuted());
        fxAreaButton.onClick = [this] { panel.showTrackPluginDialog (trackIndex); };

        soloButton.onClick = [this]
        {
            panel.commitAllNameEdits();
            audioEngine.setTrackSolo (trackIndex, soloButton.getToggleState());
            panel.updateTrackButtonStyles (*this);
        };
        muteButton.onClick = [this]
        {
            panel.commitAllNameEdits();
            audioEngine.setTrackMute (trackIndex, muteButton.getToggleState());
            panel.updateTrackButtonStyles (*this);
        };

        panSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        panSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        panSlider.setRange (0.0, 1.0, 0.01);
        panSlider.onValueChange = [this]
        {
            panel.commitAllNameEdits();
            audioEngine.setTrackPan (trackIndex, (float) panSlider.getValue());
        };

        applyMixerFaderStyle (faderSlider, faderLookAndFeel);
        faderSlider.onValueChange = [this]
        {
            panel.commitAllNameEdits();
            const auto gainDb = readSnappedFaderDb (faderSlider);
            updateFaderWithZeroSnap (faderSlider, gainDb);
            audioEngine.setTrackGain (trackIndex, gainDb);
        };

        addAndMakeVisible (nameLabel);
        addAndMakeVisible (colourButton);
        addAndMakeVisible (removeButton);
        addAndMakeVisible (inputButton);
        addAndMakeVisible (soloButton);
        addAndMakeVisible (muteButton);
        addAndMakeVisible (panSlider);
        addAndMakeVisible (faderSlider);
        addAndMakeVisible (fxAreaButton);

        syncFromEngine();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() || e.originalComponent != this)
            return;

        if (! isInteractiveHit (e.getPosition()))
            panel.showTrackPluginDialog (trackIndex);
    }

    ~TrackRowComponent() override
    {
        faderSlider.setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        const auto panelColour = audioEngine.getTrackPanelColour (trackIndex);
        const auto panelBounds = getLocalBounds().reduced (4, 2).toFloat();

        g.setColour (panelColour.withAlpha (0.22f));
        g.fillRoundedRectangle (panelBounds, 8.0f);

        g.setColour (panelColour);
        g.fillRoundedRectangle (panelBounds.getX(), panelBounds.getY() + 6.0f, 4.0f, panelBounds.getHeight() - 12.0f, 2.0f);

        g.setColour (DrizzleTheme::panelBorder().withAlpha (0.65f));
        g.drawRoundedRectangle (panelBounds, 8.0f, 1.0f);

        auto meterArea = faderSlider.getBounds().translated (-kFaderMeterGap, 0)
                            .withWidth (kFaderMeterWidth)
                            .withHeight (faderSlider.getHeight());

        if (! meterArea.isEmpty())
            paintVerticalMeter (g, meterArea, meterLevel);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (10, 8);

        auto faderStrip = bounds.removeFromRight (kFaderWidth + kFaderMeterGap + kFaderMeterWidth);
        faderSlider.setBounds (faderStrip.removeFromRight (kFaderWidth));

        auto header = bounds.removeFromTop (22);
        colourButton.setBounds (header.removeFromLeft (20).reduced (1));
        header.removeFromLeft (4);
        removeButton.setBounds (header.removeFromRight (24).reduced (2));
        nameLabel.setBounds (header);

        auto sub = bounds.removeFromTop (18);
        inputButton.setBounds (sub);

        auto controls = bounds.removeFromBottom (26);
        fxAreaButton.setBounds (bounds.reduced (4));
        soloButton.setBounds (controls.removeFromLeft (26));
        muteButton.setBounds (controls.removeFromLeft (26));
        panSlider.setBounds (controls.removeFromLeft (38).reduced (0, 2));
    }

    void syncFromEngine()
    {
        if (! nameLabel.isBeingEdited())
            nameLabel.setText (audioEngine.getTrackName (trackIndex), juce::dontSendNotification);

        inputButton.setButtonText (audioEngine.getTrackInputLabel (trackIndex));
        faderSlider.setValue (audioEngine.getTrackGain (trackIndex), juce::dontSendNotification);
        panSlider.setValue (audioEngine.getTrackPan (trackIndex), juce::dontSendNotification);
        soloButton.setToggleState (audioEngine.getTrackSolo (trackIndex), juce::dontSendNotification);
        muteButton.setToggleState (audioEngine.getTrackMute (trackIndex), juce::dontSendNotification);
        colourButton.setColour (audioEngine.getTrackPanelColour (trackIndex));
        panel.updateTrackButtonStyles (*this);

        removeButton.setVisible (audioEngine.getNumTracks() > TrackMixerProcessor::minTracks);

        const int numFx = audioEngine.getPluginChain().getNumTrackPlugins (trackIndex);
        fxAreaButton.setButtonText (numFx > 0 ? "VST " + juce::String (numFx) : "VST");
    }

    void setMeterLevel (float level) noexcept { meterLevel = level; }

    int trackIndex = 0;
    float meterLevel = 0.0f;

    juce::Label nameLabel;
    TrackColourSwatchButton colourButton;
    juce::TextButton removeButton;
    juce::TextButton inputButton;
    juce::TextButton fxAreaButton;
    juce::TextButton soloButton { "S" };
    juce::TextButton muteButton { "M" };
    juce::Slider panSlider;
    juce::Slider faderSlider;

private:
    TrackMixerPanel& panel;
    AudioEngine& audioEngine;
    MixerFaderLookAndFeel& faderLookAndFeel;
};

//==============================================================================
TrackMixerPanel::TrackMixerPanel (AudioEngine& engine)
    : audioEngine (engine)
{
    tracksListContent = std::make_unique<TracksListContent> (*this);
    tracksViewport.setViewedComponent (tracksListContent.get(), false);
    tracksViewport.setScrollBarsShown (true, false);
    tracksViewport.setScrollBarThickness (12);
    tracksViewport.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, DrizzleTheme::accent().withAlpha (0.7f));
    tracksViewport.getVerticalScrollBar().setColour (juce::ScrollBar::trackColourId, DrizzleTheme::panelBackground().darker (0.2f));

    addTrackButton.setColour (juce::TextButton::buttonColourId, DrizzleTheme::accent().withAlpha (0.35f));
    addTrackButton.setColour (juce::TextButton::textColourOffId, DrizzleTheme::textPrimary());
    addTrackButton.onClick = [this]
    {
        commitAllNameEdits();

        if (audioEngine.addTrack())
            rebuildTrackList();
    };

    masterTitle.setText (juce::String::fromUTF8 (u8"\u30e1\u30a4\u30f3\u51fa\u529b"), juce::dontSendNotification);
    DrizzleTheme::applyLabel (masterTitle);
    applyMixerFaderStyle (masterFader, faderLookAndFeel);
    masterFader.setValue (audioEngine.getMasterGainDb(), juce::dontSendNotification);
    masterFader.onValueChange = [this]
    {
        commitAllNameEdits();
        const auto gainDb = readSnappedFaderDb (masterFader);
        updateFaderWithZeroSnap (masterFader, gainDb);
        audioEngine.setMasterGainDb (gainDb);
    };
    masterMuteButton.setClickingTogglesState (true);
    masterMuteButton.onClick = [this]
    {
        commitAllNameEdits();
        audioEngine.setMasterMute (masterMuteButton.getToggleState());
        updateMasterButtonStyles();
    };
    masterMonoButton.setClickingTogglesState (true);
    masterMonoButton.onClick = [this]
    {
        commitAllNameEdits();
        audioEngine.setMasterMono (masterMonoButton.getToggleState());
        updateMasterButtonStyles();
    };
    updateMasterButtonStyles();

    addAndMakeVisible (tracksViewport);
    addAndMakeVisible (addTrackButton);
    addAndMakeVisible (masterTitle);
    addAndMakeVisible (masterFader);
    addAndMakeVisible (masterMuteButton);
    addAndMakeVisible (masterMonoButton);

    rebuildTrackList();
    startTimerHz (20);
}

int TrackMixerPanel::getNumTrackRows() const
{
    return audioEngine.getNumTracks();
}

TrackMixerPanel::~TrackMixerPanel()
{
    prepareForShutdown();
    trackRows.clear();
    masterFader.setLookAndFeel (nullptr);
}

void TrackMixerPanel::rebuildTrackList()
{
    commitAllNameEdits();
    trackRows.clear();
    tracksListContent->removeAllChildren();

    for (int i = 0; i < audioEngine.getNumTracks(); ++i)
    {
        auto* row = trackRows.add (new TrackRowComponent (*this, audioEngine, faderLookAndFeel, i));
        tracksListContent->addAndMakeVisible (row);
    }

    layoutTracksList();
    updateAddTrackButton();
    resized();
}

void TrackMixerPanel::layoutTracksList()
{
    for (int i = 0; i < trackRows.size(); ++i)
    {
        trackRows.getUnchecked (i)->setBounds (0,
                                               i * kTrackRowHeight,
                                               tracksListContent->getWidth(),
                                               kTrackRowContentHeight);
    }

    tracksListContent->setSize (tracksListContent->getWidth(), tracksListContent->getContentHeight());
}

void TrackMixerPanel::updateAddTrackButton()
{
    const bool canAdd = audioEngine.getNumTracks() < TrackMixerProcessor::maxTracks;
    addTrackButton.setVisible (canAdd);
    addTrackButton.setEnabled (canAdd);
}

void TrackMixerPanel::commitAllNameEdits()
{
    for (auto* row : trackRows)
        if (row->nameLabel.isBeingEdited())
            row->nameLabel.hideEditor (false);
}

void TrackMixerPanel::prepareForShutdown()
{
    stopTimer();
    commitAllNameEdits();
}

void TrackMixerPanel::updateTrackButtonStyles (TrackRowComponent& row)
{
    DrizzleTheme::applyTrackToggleButton (row.soloButton,
                                          audioEngine.getTrackSolo (row.trackIndex),
                                          false);
    DrizzleTheme::applyTrackToggleButton (row.muteButton,
                                          audioEngine.getTrackMute (row.trackIndex),
                                          true);
}

void TrackMixerPanel::updateMasterButtonStyles()
{
    DrizzleTheme::applyTrackToggleButton (masterMuteButton, audioEngine.getMasterMute(), true);
    DrizzleTheme::applyTrackToggleButton (masterMonoButton, audioEngine.getMasterMono(), false);
}

void TrackMixerPanel::reloadFromEngine()
{
    rebuildTrackList();
    syncUIFromEngine();
}

void TrackMixerPanel::syncUIFromEngine()
{
    for (auto* row : trackRows)
        row->syncFromEngine();

    masterFader.setValue (audioEngine.getMasterGainDb(), juce::dontSendNotification);
    masterMuteButton.setToggleState (audioEngine.getMasterMute(), juce::dontSendNotification);
    masterMonoButton.setToggleState (audioEngine.getMasterMono(), juce::dontSendNotification);
    updateMasterButtonStyles();
    updateAddTrackButton();
}

void TrackMixerPanel::showTrackColourPicker (int trackIndex, juce::Component& anchor)
{
    const auto initialColour = audioEngine.getTrackPanelColour (trackIndex);

    auto picker = std::make_unique<TrackColourPickerContent> (initialColour,
                                                              [this, trackIndex] (juce::Colour colour)
                                                              {
                                                                  audioEngine.setTrackPanelColour (trackIndex, colour);

                                                                  if (juce::isPositiveAndBelow (trackIndex, trackRows.size()))
                                                                  {
                                                                      trackRows.getUnchecked (trackIndex)->syncFromEngine();
                                                                      trackRows.getUnchecked (trackIndex)->repaint();
                                                                  }
                                                              });

    juce::CallOutBox::launchAsynchronously (std::move (picker), anchor.getScreenBounds(), nullptr);
}

void TrackMixerPanel::confirmRemoveTrack (int trackIndex)
{
    if (audioEngine.getNumTracks() <= TrackMixerProcessor::minTracks)
        return;

    const auto trackName = audioEngine.getTrackName (trackIndex);
    const auto message = juce::String::fromUTF8 (u8"\u30c8\u30e9\u30c3\u30af\u300c") + trackName
                       + juce::String::fromUTF8 (u8"\u300d\u3092\u524a\u9664\u3057\u307e\u3059\u304b\uff1f");

    juce::NativeMessageBox::showOkCancelBox (juce::MessageBoxIconType::WarningIcon,
                                             juce::String::fromUTF8 (u8"\u30c8\u30e9\u30c3\u30af\u524a\u9664"),
                                             message,
                                             this,
                                             juce::ModalCallbackFunction::create ([this, trackIndex] (int result)
    {
        if (result != 0 && audioEngine.removeTrack (trackIndex))
            rebuildTrackList();
    }));
}

void TrackMixerPanel::showInputSelectionForTrack (int trackIndex)
{
    commitAllNameEdits();

    auto panel = std::make_unique<InputSelectionPanel> (audioEngine, trackIndex, [this]
    {
        syncUIFromEngine();
    });

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = juce::String::fromUTF8 (u8"\u5165\u529b\u9078\u629e");
    options.content.setOwned (panel.release());
    options.componentToCentreAround = this;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.dialogBackgroundColour = DrizzleTheme::panelBackground();
    options.launchAsync();
}

void TrackMixerPanel::showTrackPluginDialog (int trackIndex)
{
    commitAllNameEdits();

    struct DialogHolder final : public juce::Component
    {
        explicit DialogHolder (std::unique_ptr<TrackPluginPanel> contentIn)
            : content (std::move (contentIn))
        {
            addAndMakeVisible (*content);
        }

        void resized() override
        {
            if (content != nullptr)
                content->setBounds (getLocalBounds());
        }

        std::unique_ptr<TrackPluginPanel> content;
    };

    auto* window = new juce::DialogWindow (juce::String::fromUTF8 (u8"\u30c8\u30e9\u30c3\u30af VST"),
                                           DrizzleTheme::panelBackground(),
                                           true);

    auto panel = std::make_unique<TrackPluginPanel> (audioEngine,
                                                     trackIndex,
                                                     [window, this]
                                                     {
                                                         audioEngine.getPluginChain().getTrackPluginManager().hideAllEditors();
                                                         window->exitModalState (0);
                                                         delete window;
                                                         syncUIFromEngine();
                                                     });

    window->setContentOwned (new DialogHolder (std::move (panel)), true);
    window->centreWithSize (720, 520);
    window->enterModalState (true, nullptr, false);
}

void TrackMixerPanel::timerCallback()
{
    for (auto* row : trackRows)
        row->setMeterLevel (audioEngine.getTrackPeakLevel (row->trackIndex));

    masterLevel = audioEngine.getMasterPeakLevel();

    for (auto* row : trackRows)
        row->repaint();

    repaint();
}

void TrackMixerPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    DrizzleTheme::paintPanel (g, bounds, juce::String::fromUTF8 (u8"\u30c8\u30e9\u30c3\u30af"));

    auto masterMeter = masterFader.getBounds().translated (-kFaderMeterGap, 0).withWidth (kFaderMeterWidth);
    if (! masterMeter.isEmpty())
        paintVerticalMeter (g, masterMeter, masterLevel);
}

void TrackMixerPanel::resized()
{
    auto area = getLocalBounds().reduced (6);
    area.removeFromTop (kPanelTitleHeight);

    auto masterArea = area.removeFromBottom (kMasterSectionHeight).reduced (4, 2);
    if (addTrackButton.isVisible())
        addTrackButton.setBounds (area.removeFromBottom (kAddTrackButtonHeight).reduced (2, 4));

    tracksViewport.setBounds (area.reduced (2));

    const int viewportWidth = juce::jmax (1, tracksViewport.getWidth());
    tracksListContent->setSize (viewportWidth, tracksListContent->getContentHeight());
    layoutTracksList();

    auto masterFaderStrip = masterArea.removeFromRight (kFaderWidth + kFaderMeterGap + kFaderMeterWidth);
    masterFader.setBounds (masterFaderStrip.removeFromRight (kFaderWidth));
    masterTitle.setBounds (masterArea.removeFromTop (20));
    auto masterControls = masterArea.removeFromBottom (26);
    masterMuteButton.setBounds (masterControls.removeFromLeft (60));
    masterMonoButton.setBounds (masterControls.removeFromLeft (60));
}

//==============================================================================
StreamPreviewPanel::StreamPreviewPanel (AudioEngine& engine)
    : audioEngine (engine)
{
    audioEngine.getStreamEngine().addChangeListener (this);
    audioEngine.getYoutubeChatClient().addChangeListener (this);

    youtubeLiveLink.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    youtubeLiveLink.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    youtubeLiveLink.setColour (juce::TextButton::textColourOffId, DrizzleTheme::accent());
    youtubeLiveLink.setColour (juce::TextButton::textColourOnId, DrizzleTheme::accent().brighter (0.2f));
    youtubeLiveLink.onClick = [this]
    {
        if (youtubeLiveUrl.isWellFormed())
            DrizzleTheme::launchUrlInChrome (youtubeLiveUrl);
    };
    addAndMakeVisible (youtubeLiveLink);

    updateYoutubeLiveLink();
    startTimerHz (8);
}

StreamPreviewPanel::~StreamPreviewPanel()
{
    audioEngine.getStreamEngine().removeChangeListener (this);
    audioEngine.getYoutubeChatClient().removeChangeListener (this);
    stopTimer();
}

void StreamPreviewPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    DrizzleTheme::paintPanel (g, bounds, juce::String::fromUTF8 (u8"\u914d\u4fe1\u30d7\u30ec\u30d3\u30e5\u30fc"));

    auto content = bounds.reduced (8);
    content.removeFromTop (28);
    auto preview = content.removeFromTop (content.getHeight() - 36).reduced (2);
    auto meterArea = content;

    const auto& streamEngine = audioEngine.getStreamEngine();
    const auto state = streamEngine.getState();
    const bool isLive = state == StreamState::live || state == StreamState::starting;
    const auto title = streamEngine.getConfig().title;

    g.setColour (juce::Colour (0xff2b3140));
    g.fillRoundedRectangle (preview.toFloat(), 6.0f);

    g.setColour (DrizzleTheme::textMuted());
    g.setFont (juce::FontOptions { 22.0f }.withStyle ("Bold"));
    g.drawText (title.isNotEmpty() ? title : juce::String ("Drizzle LIVE STREAMING"),
                preview,
                juce::Justification::centred);

    if (isLive)
    {
        g.setColour (DrizzleTheme::accentRed());
        g.fillEllipse (preview.getRight() - 120.0f, preview.getY() + 12.0f, 10.0f, 10.0f);
        g.setColour (DrizzleTheme::textPrimary());
        g.setFont (juce::FontOptions { 14.0f }.withStyle ("Bold"));
        g.drawText ("LIVE  " + liveTimer, preview.removeFromTop (36).removeFromRight (140),
                    juce::Justification::centredRight);
    }

    paintHorizontalMeter (g, meterArea, masterLevel);
    g.setColour (DrizzleTheme::textMuted());
    const auto dbText = masterLevel > 0.0001f
                          ? juce::String (MixerDbScale::linearToDb (masterLevel), 1) + " dB"
                          : juce::String::fromUTF8 (u8"-inf dB");
    g.drawText (dbText, meterArea.removeFromRight (56), juce::Justification::centredRight);
}

void StreamPreviewPanel::resized()
{
    auto header = getLocalBounds().reduced (8).removeFromTop (28);
    header.removeFromLeft (juce::roundToInt ((float) header.getWidth() * 0.45f));
    youtubeLiveLink.setBounds (header.reduced (4, 2));
}

void StreamPreviewPanel::timerCallback()
{
    const auto& streamEngine = audioEngine.getStreamEngine();
    masterLevel = streamEngine.getState() == StreamState::live
                    ? streamEngine.getOutputPeakLevel()
                    : audioEngine.getMasterPeakLevel();
    liveTimer = formatLiveDuration (streamEngine.getLiveDurationSeconds());
    updateYoutubeLiveLink();
    repaint();
}

void StreamPreviewPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateYoutubeLiveLink();
    repaint();
}

void StreamPreviewPanel::updateYoutubeLiveLink()
{
    const auto& streamEngine = audioEngine.getStreamEngine();
    const auto state = streamEngine.getState();
    const bool isLive = state == StreamState::live || state == StreamState::starting;

    if (! isLive)
    {
        youtubeLiveUrl = {};
        youtubeLiveLink.setVisible (false);
        return;
    }

    const auto& chatClient = audioEngine.getYoutubeChatClient();
    auto urlString = chatClient.getLiveStudioUrl();

    if (urlString.isEmpty())
        urlString = chatClient.getLiveWatchUrl();

    if (urlString.isEmpty())
        urlString = "https://studio.youtube.com/";

    youtubeLiveUrl = juce::URL (urlString);
    youtubeLiveLink.setVisible (true);
}

//==============================================================================
StreamSettingsPanel::StreamSettingsPanel (AudioEngine& engine)
    : audioEngine (engine)
{
    serviceLabel.setText (juce::String::fromUTF8 (u8"\u914d\u4fe1\u30b5\u30fc\u30d3\u30b9"), juce::dontSendNotification);
    titleLabel.setText (juce::String::fromUTF8 (u8"\u30bf\u30a4\u30c8\u30eb"), juce::dontSendNotification);
    streamKeyLabel.setText (juce::String::fromUTF8 (u8"\u30b9\u30c8\u30ea\u30fc\u30e0\u30ad\u30fc"
                                                    u8" (YouTube Studio \u306e\u30ad\u30fc\u306e\u307f)"),
                            juce::dontSendNotification);
    DrizzleTheme::applyLabel (serviceLabel);
    DrizzleTheme::applyLabel (titleLabel);
    DrizzleTheme::applyLabel (streamKeyLabel);

    serviceBox.addItem ("YouTube Live", 1);
    serviceBox.setSelectedId (1, juce::dontSendNotification);

    titleEditor.setMultiLine (false);
    streamKeyEditor.setMultiLine (false);
    streamKeyEditor.setPasswordCharacter ((juce::juce_wchar) 0x2022);
    streamKeyEditor.setTextToShowWhenEmpty (juce::String::fromUTF8 (u8"xxxx-xxxx-xxxx-xxxx"),
                                            DrizzleTheme::textMuted());

    streamButton.setColour (juce::TextButton::buttonColourId, DrizzleTheme::accentRed());

    testButton.onClick = [this] { onTestClicked(); };
    streamButton.onClick = [this] { onStreamClicked(); };
    titleEditor.onFocusLost = [this] { applyConfigFromUI(); };
    titleEditor.onReturnKey = [this] { applyConfigFromUI(); };
    streamKeyEditor.onFocusLost = [this] { applyConfigFromUI(); };
    streamKeyEditor.onReturnKey = [this] { applyConfigFromUI(); };
    serviceBox.onChange = [this] { applyConfigFromUI(); };

    addAndMakeVisible (serviceLabel);
    addAndMakeVisible (serviceBox);
    addAndMakeVisible (titleLabel);
    addAndMakeVisible (titleEditor);
    addAndMakeVisible (streamKeyLabel);
    addAndMakeVisible (streamKeyEditor);
    addAndMakeVisible (testButton);
    addAndMakeVisible (streamButton);

    audioEngine.getStreamEngine().addChangeListener (this);
    reloadFromEngine();
}

StreamSettingsPanel::~StreamSettingsPanel()
{
    audioEngine.getStreamEngine().removeChangeListener (this);
}

void StreamSettingsPanel::reloadFromEngine()
{
    const auto config = audioEngine.getStreamEngine().getConfig();
    titleEditor.setText (config.title, juce::dontSendNotification);
    streamKeyEditor.setText (config.streamKey, juce::dontSendNotification);
    serviceBox.setSelectedId (config.serviceId > 0 ? config.serviceId : 1, juce::dontSendNotification);
    updateStreamButton();
}

void StreamSettingsPanel::prepareForShutdown()
{
    applyConfigFromUI();
}

void StreamSettingsPanel::applyConfigFromUI()
{
    StreamConfig config;
    config.title = titleEditor.getText().trim();
    config.streamKey = streamKeyEditor.getText().trim();
    config.serviceId = serviceBox.getSelectedId();
    audioEngine.getStreamEngine().setConfig (config);
    audioEngine.saveSessionSettings();
}

void StreamSettingsPanel::updateStreamButton()
{
    const auto state = audioEngine.getStreamEngine().getState();
    const bool streaming = state == StreamState::live || state == StreamState::starting;

    streamButton.setButtonText (streaming
                                    ? juce::String::fromUTF8 (u8"\u914d\u4fe1\u505c\u6b62")
                                    : juce::String::fromUTF8 (u8"\u914d\u4fe1\u958b\u59cb"));
    streamButton.setEnabled (state != StreamState::stopping);
    testButton.setEnabled (! streaming);
    titleEditor.setReadOnly (streaming);
    streamKeyEditor.setReadOnly (streaming);
    serviceBox.setEnabled (! streaming);
}

void StreamSettingsPanel::onTestClicked()
{
    applyConfigFromUI();

    if (audioEngine.getStreamEngine().testConnection())
    {
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                    juce::String::fromUTF8 (u8"\u63a5\u7d9a\u30c6\u30b9\u30c8"),
                                                    juce::String::fromUTF8 (u8"FFmpeg \u3068\u30b9\u30c8\u30ea\u30fc\u30e0\u30ad\u30fc\u306e\u78ba\u8a8d\u304c\u5b8c\u4e86\u3057\u307e\u3057\u305f\u3002"));
    }
    else
    {
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    juce::String::fromUTF8 (u8"\u63a5\u7d9a\u30c6\u30b9\u30c8"),
                                                    audioEngine.getStreamEngine().getLastError());
    }
}

void StreamSettingsPanel::onStreamClicked()
{
    applyConfigFromUI();

    auto& streamEngine = audioEngine.getStreamEngine();
    const auto state = streamEngine.getState();

    if (state == StreamState::live || state == StreamState::starting)
    {
        audioEngine.stopStreaming();
        return;
    }

    audioEngine.getYoutubeChatClient().setActiveStreamKey (streamEngine.getConfig().streamKey);

    if (! streamEngine.startStream())
    {
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    juce::String::fromUTF8 (u8"\u914d\u4fe1\u958b\u59cb"),
                                                    streamEngine.getLastError());
    }
}

void StreamSettingsPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateStreamButton();
}

void StreamSettingsPanel::paint (juce::Graphics& g)
{
    DrizzleTheme::paintPanel (g, getLocalBounds(), juce::String::fromUTF8 (u8"\u914d\u4fe1\u8a2d\u5b9a"));

    auto area = getLocalBounds().reduced (8);
    area.removeFromTop (28);
    area.removeFromLeft (area.getWidth() * 2 / 3);
    auto scenes = area.removeFromLeft (area.getWidth() / 2).reduced (6, 8);
    auto sources = area.reduced (6, 8);

    g.setColour (DrizzleTheme::textMuted());
    g.setFont (juce::FontOptions { 12.0f }.withStyle ("Bold"));
    g.drawText (juce::String::fromUTF8 (u8"\u30b7\u30fc\u30f3"), scenes.removeFromTop (18), juce::Justification::centredLeft);
    g.drawText (juce::String::fromUTF8 (u8"\u30bd\u30fc\u30b9"), sources.removeFromTop (18), juce::Justification::centredLeft);

    g.setFont (juce::FontOptions { 13.0f });
    g.setColour (DrizzleTheme::textPrimary());
    const juce::StringArray sceneItems {
        juce::String::fromUTF8 (u8"\u30e1\u30a4\u30f3\u30b7\u30fc\u30f3"),
        juce::String::fromUTF8 (u8"\u30c8\u30fc\u30af\u30b7\u30fc\u30f3"),
        juce::String::fromUTF8 (u8"\u6f14\u594f\u30b7\u30fc\u30f3"),
        juce::String::fromUTF8 (u8"\u5f85\u6a5f\u753b\u9762")
    };
    const juce::StringArray sourceItems {
        juce::String::fromUTF8 (u8"\u6620\u50cf\u30ad\u30e3\u30d7\u30c1\u30e3"),
        "logo.png",
        juce::String::fromUTF8 (u8"\u30bf\u30a4\u30c8\u30eb"),
        juce::String::fromUTF8 (u8"\u30a6\u30a3\u30f3\u30c9\u30a6")
    };

    int y = 0;
    for (const auto& item : sceneItems)
    {
        auto row = scenes.removeFromTop (22);
        g.setColour (item == sceneItems[0] ? DrizzleTheme::accent().withAlpha (0.3f) : juce::Colours::transparentBlack);
        g.fillRect (row);
        g.setColour (DrizzleTheme::textPrimary());
        g.drawText (item, row, juce::Justification::centredLeft);
    }

    for (const auto& item : sourceItems)
    {
        auto row = sources.removeFromTop (22);
        g.drawText (item, row, juce::Justification::centredLeft);
    }
}

void StreamSettingsPanel::resized()
{
    auto area = getLocalBounds().reduced (8);
    area.removeFromTop (28);

    auto left = area.removeFromLeft (area.getWidth() / 3).reduced (4);

    serviceLabel.setBounds (left.removeFromTop (20));
    serviceBox.setBounds (left.removeFromTop (28));
    left.removeFromTop (8);
    titleLabel.setBounds (left.removeFromTop (20));
    titleEditor.setBounds (left.removeFromTop (28));
    left.removeFromTop (8);
    streamKeyLabel.setBounds (left.removeFromTop (20));
    streamKeyEditor.setBounds (left.removeFromTop (28));
    left.removeFromTop (12);
    testButton.setBounds (left.removeFromTop (30));
    streamButton.setBounds (left.removeFromBottom (40));
}

//==============================================================================
namespace
{
class YoutubeConnectDialog final : public juce::Component
{
public:
    YoutubeConnectDialog (YoutubeChatClient& chatClientIn, std::function<void()> onCloseIn)
        : chatClient (chatClientIn),
          onClose (std::move (onCloseIn))
    {
        setSize (460, 280);

        titleLabel.setText (juce::String::fromUTF8 (u8"YouTube \u30b3\u30e1\u30f3\u30c8\u9023\u643a\u8a2d\u5b9a"),
                            juce::dontSendNotification);
        DrizzleTheme::applyLabel (titleLabel);

        helpLabel.setText (
            juce::String::fromUTF8 (
                u8"Google Cloud \u3067 YouTube Data API v3 \u3092\u6709\u52b9\u5316\u3057\u3001"
                u8"OAuth \u30af\u30e9\u30a4\u30a2\u30f3\u30c8\uff08\u30a6\u30a7\u30d6\u30a2\u30d7\u30ea\uff09\u3092\u4f5c\u6210\u3057\u3066\u304f\u3060\u3055\u3044\u3002\n"
                u8"\u627f\u8a8d\u30ea\u30c0\u30a4\u30ec\u30af\u30c8 URI: http://127.0.0.1:8765/\n"
                u8"OAuth \u540c\u610f\u753b\u9762\u3067\u30c6\u30b9\u30c8\u30e6\u30fc\u30b6\u30fc\u306b\u81ea\u5206\u306e Gmail \u3092\u8ffd\u52a0\u3057\u3066\u304f\u3060\u3055\u3044\u3002"),
            juce::dontSendNotification);
        helpLabel.setJustificationType (juce::Justification::topLeft);
        DrizzleTheme::applyLabel (helpLabel, true);

        clientIdLabel.setText ("Client ID", juce::dontSendNotification);
        clientSecretLabel.setText ("Client Secret", juce::dontSendNotification);
        DrizzleTheme::applyLabel (clientIdLabel);
        DrizzleTheme::applyLabel (clientSecretLabel);

        const auto config = chatClient.getApiConfig();
        clientIdEditor.setText (config.clientId);
        clientSecretEditor.setText (config.clientSecret);
        clientSecretEditor.setPasswordCharacter ((juce::juce_wchar) 0x2022);

        connectButton.setButtonText (juce::String::fromUTF8 (u8"Google\u30a2\u30ab\u30a6\u30f3\u30c8\u9023\u643a"));
        cancelButton.setButtonText (juce::String::fromUTF8 (u8"\u9589\u3058\u308b"));

        connectButton.onClick = [this] { onConnectClicked(); };
        cancelButton.onClick = [this]
        {
            if (onClose != nullptr)
                onClose();
        };

        addAndMakeVisible (titleLabel);
        addAndMakeVisible (helpLabel);
        addAndMakeVisible (clientIdLabel);
        addAndMakeVisible (clientIdEditor);
        addAndMakeVisible (clientSecretLabel);
        addAndMakeVisible (clientSecretEditor);
        addAndMakeVisible (connectButton);
        addAndMakeVisible (cancelButton);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (16);
        titleLabel.setBounds (area.removeFromTop (24));
        helpLabel.setBounds (area.removeFromTop (56));
        area.removeFromTop (8);
        clientIdLabel.setBounds (area.removeFromTop (18));
        clientIdEditor.setBounds (area.removeFromTop (28));
        area.removeFromTop (6);
        clientSecretLabel.setBounds (area.removeFromTop (18));
        clientSecretEditor.setBounds (area.removeFromTop (28));
        area.removeFromTop (12);
        auto buttons = area.removeFromTop (30);
        cancelButton.setBounds (buttons.removeFromRight (90));
        connectButton.setBounds (buttons.removeFromRight (160));
    }

private:
    void onConnectClicked()
    {
        YoutubeApiConfig config;
        config.clientId = clientIdEditor.getText().trim();
        config.clientSecret = clientSecretEditor.getText().trim();
        config.redirectPort = 8765;
        chatClient.setApiConfig (config);

        connectButton.setEnabled (false);

        chatClient.beginOAuthFlow ([this] (bool success, const juce::String& message)
        {
            connectButton.setEnabled (true);

            if (! success)
            {
                juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                            juce::String::fromUTF8 (u8"YouTube \u9023\u643a"),
                                                            message);
                return;
            }

            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                        juce::String::fromUTF8 (u8"YouTube \u9023\u643a"),
                                                        juce::String::fromUTF8 (u8"\u9023\u643a\u304c\u5b8c\u4e86\u3057\u307e\u3057\u305f\u3002"));

            if (onClose != nullptr)
                onClose();
        });
    }

    YoutubeChatClient& chatClient;
    std::function<void()> onClose;
    juce::Label titleLabel;
    juce::Label helpLabel;
    juce::Label clientIdLabel;
    juce::TextEditor clientIdEditor;
    juce::Label clientSecretLabel;
    juce::TextEditor clientSecretEditor;
    juce::TextButton connectButton;
    juce::TextButton cancelButton;
};
} // namespace

CommentPanel::CommentPanel (AudioEngine& engine)
    : audioEngine (engine)
{
    commentDisplay.setMultiLine (true);
    commentDisplay.setReadOnly (true);
    commentDisplay.setScrollbarsShown (true);
    commentDisplay.setFont (juce::FontOptions { 13.0f });

    DrizzleTheme::applyLabel (viewerLabel, true);
    DrizzleTheme::applyLabel (statusLabel, true);
    commentInput.setTextToShowWhenEmpty (juce::String::fromUTF8 (u8"\u30e1\u30c3\u30bb\u30fc\u30b8\u3092\u5165\u529b..."), DrizzleTheme::textMuted());

    sendButton.onClick = [this] { onSendClicked(); };
    authButton.onClick = [this] { onAuthClicked(); };
    commentInput.onReturnKey = [this] { onSendClicked(); };

    addAndMakeVisible (authButton);
    addAndMakeVisible (commentDisplay);
    addAndMakeVisible (commentInput);
    addAndMakeVisible (sendButton);
    addAndMakeVisible (viewerLabel);
    addAndMakeVisible (statusLabel);

    audioEngine.getYoutubeChatClient().addChangeListener (this);
    audioEngine.getStreamEngine().addChangeListener (this);

    refreshComments();
    updateViewerLabel();
    updateAuthButton();
    syncChatPollingWithStream();
    startTimerHz (2);
}

CommentPanel::~CommentPanel()
{
    audioEngine.getYoutubeChatClient().removeChangeListener (this);
    audioEngine.getStreamEngine().removeChangeListener (this);
    stopTimer();
}

void CommentPanel::prepareForShutdown()
{
    audioEngine.getYoutubeChatClient().stopPolling();
}

void CommentPanel::paint (juce::Graphics& g)
{
    DrizzleTheme::paintPanel (g, getLocalBounds(), "YouTube");
}

void CommentPanel::resized()
{
    auto area = getLocalBounds().reduced (8);
    area.removeFromTop (28);

    auto header = area.removeFromTop (28);
    authButton.setBounds (header.removeFromRight (110).reduced (0, 2));
    statusLabel.setBounds (header.reduced (0, 4));

    viewerLabel.setBounds (area.removeFromBottom (22));
    auto inputRow = area.removeFromBottom (34);
    sendButton.setBounds (inputRow.removeFromRight (64));
    commentInput.setBounds (inputRow.reduced (0, 2));
    commentDisplay.setBounds (area.reduced (2));
}

void CommentPanel::timerCallback()
{
    updateViewerLabel();
    syncChatPollingWithStream();
}

void CommentPanel::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &audioEngine.getYoutubeChatClient())
    {
        refreshComments();
        updateViewerLabel();
        updateAuthButton();
        statusLabel.setText (audioEngine.getYoutubeChatClient().getStatusText(), juce::dontSendNotification);
    }

    if (source == &audioEngine.getStreamEngine())
    {
        syncChatPollingWithStream();
        refreshComments();
    }
}

void CommentPanel::refreshComments()
{
    juce::String text;
    const auto messages = audioEngine.getYoutubeChatClient().getMessages();

    for (const auto& message : messages)
    {
        if (text.isNotEmpty())
            text += "\n";

        text += message.author + ": " + message.text;
    }

    if (text.isEmpty())
    {
        const auto& chatClient = audioEngine.getYoutubeChatClient();
        const auto streamState = audioEngine.getStreamEngine().getState();
        const bool streaming = streamState == StreamState::live || streamState == StreamState::starting;

        if (! chatClient.isAuthenticated())
        {
            text = juce::String::fromUTF8 (u8"YouTube \u9023\u643a\u30dc\u30bf\u30f3\u304b\u3089\u30a2\u30ab\u30a6\u30f3\u30c8\u3092\u9023\u643a\u3057\u3066\u304f\u3060\u3055\u3044\u3002");
        }
        else if (! streaming)
        {
            text = juce::String::fromUTF8 (u8"\u914d\u4fe1\u3057\u3066\u3044\u307e\u305b\u3093\u3002");
        }
        else
        {
            const auto status = chatClient.getStatusText();

            text = status.isNotEmpty()

                       ? status

                       : juce::String::fromUTF8 (u8"\u30e9\u30a4\u30d6\u30c1\u30e3\u30c3\u30c8\u63a5\u7d9a\u3092\u5f85\u6a5f\u4e2d...");
        }
    }

    if (commentDisplay.getText() != text)
    {
        commentDisplay.setText (text);
        commentDisplay.moveCaretToEnd();
    }
}

void CommentPanel::updateViewerLabel()
{
    const int viewers = audioEngine.getYoutubeChatClient().getConcurrentViewers();
    viewerLabel.setText (juce::String::fromUTF8 (u8"\u8996\u8074\u8005\u6570: ") + juce::String (viewers),
                         juce::dontSendNotification);
}

void CommentPanel::updateAuthButton()
{
    authButton.setButtonText (audioEngine.getYoutubeChatClient().isAuthenticated()
                                  ? juce::String::fromUTF8 (u8"\u9023\u643a\u89e3\u9664")
                                  : juce::String::fromUTF8 (u8"YouTube\u9023\u643a"));
}

void CommentPanel::syncChatPollingWithStream()
{
    auto& chatClient = audioEngine.getYoutubeChatClient();
    auto& streamEngine = audioEngine.getStreamEngine();
    const auto streamState = streamEngine.getState();
    const bool isLive = streamState == StreamState::live;
    const bool shouldPoll = chatClient.isAuthenticated() && isLive;

    if (shouldPoll && ! chatClient.isPolling())
        chatClient.startPolling();
    else if (! shouldPoll && chatClient.isPolling())
        chatClient.stopPolling();
}

void CommentPanel::onSendClicked()
{
    const auto text = commentInput.getText().trim();

    if (text.isEmpty())
        return;

    if (! audioEngine.getYoutubeChatClient().isAuthenticated())
    {
        showConnectDialog();
        return;
    }

    audioEngine.getYoutubeChatClient().sendChatMessage (text);
    commentInput.clear();
}

void CommentPanel::onAuthClicked()
{
    if (audioEngine.getYoutubeChatClient().isAuthenticated())
    {
        juce::NativeMessageBox::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon,
                                                 juce::String::fromUTF8 (u8"YouTube \u9023\u643a\u89e3\u9664"),
                                                 juce::String::fromUTF8 (u8"YouTube \u30a2\u30ab\u30a6\u30f3\u30c8\u9023\u643a\u3092\u89e3\u9664\u3057\u307e\u3059\u304b\uff1f"),
                                                 this,
                                                 juce::ModalCallbackFunction::create ([this] (int result)
        {
            if (result != 0)
                audioEngine.getYoutubeChatClient().disconnectAccount();
        }));

        return;
    }

    showConnectDialog();
}

void CommentPanel::showConnectDialog()
{
    struct DialogHolder : public juce::Component
    {
        explicit DialogHolder (std::unique_ptr<YoutubeConnectDialog> contentIn)
            : content (std::move (contentIn))
        {
            addAndMakeVisible (*content);
        }

        void resized() override
        {
            if (content != nullptr)
                content->setBounds (getLocalBounds());
        }

        std::unique_ptr<YoutubeConnectDialog> content;
    };

    auto* window = new juce::DialogWindow (juce::String::fromUTF8 (u8"YouTube \u30b3\u30e1\u30f3\u30c8\u9023\u643a"),
                                           DrizzleTheme::panelBackground(),
                                           true);

    auto dialog = std::make_unique<YoutubeConnectDialog> (audioEngine.getYoutubeChatClient(),
                                                          [window]
                                                          {
                                                              window->exitModalState (0);
                                                          });
    auto* dialogPtr = dialog.get();
    window->setContentOwned (new DialogHolder (std::move (dialog)), true);
    window->centreWithSize (dialogPtr->getWidth(), dialogPtr->getHeight());
    window->enterModalState (true, nullptr, true);
}

//==============================================================================
StatusBarComponent::StatusBarComponent (AudioEngine& engine)
    : audioEngine (engine)
{
    statusDisplay.setMultiLine (true);
    statusDisplay.setReadOnly (true);
    statusDisplay.setScrollbarsShown (true);
    statusDisplay.setCaretVisible (false);
    statusDisplay.setFont (juce::FontOptions { 12.0f });
    statusDisplay.setColour (juce::TextEditor::backgroundColourId,
                             DrizzleTheme::panelBackground().darker (0.2f));
    statusDisplay.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    statusDisplay.setColour (juce::TextEditor::textColourId, DrizzleTheme::textPrimary());

    DrizzleTheme::applyLabel (statsLabel, true);
    addAndMakeVisible (statusDisplay);
    addAndMakeVisible (statsLabel);
    timerCallback();
    startTimerHz (2);
}

void StatusBarComponent::timerCallback()
{
    const auto& streamEngine = audioEngine.getStreamEngine();
    juce::String statusText = streamEngine.getStatusText();

    if (streamEngine.getState() == StreamState::error)
    {
        const auto errorText = streamEngine.getLastError();

        if (errorText.isNotEmpty())
            statusText = errorText;
    }

    if (! DrizzleVst3Host::isLicenseCompatProcess())
    {
        statusText = juce::String::fromUTF8 (u8"[\u30e9\u30a4\u30bb\u30f3\u30xb9] reaper.exe \u304b\u3089\u8d77\u52d5\u3057\u3066\u304f\u3060\u3055\u3044 \u2014 ")
                   + statusText;
    }

    if (statusDisplay.getText() != statusText)
        statusDisplay.setText (statusText);

    const int drops = streamEngine.getUnderrunCount();
    statsLabel.setText ("Drop " + juce::String (drops)
                        + juce::String::fromUTF8 (u8"   \u30d5\u30ec\u30fc\u30e0 30 fps"),
                        juce::dontSendNotification);
}

void StatusBarComponent::paint (juce::Graphics& g)
{
    g.setColour (DrizzleTheme::panelBackground().darker (0.15f));
    g.fillRect (getLocalBounds());
    g.setColour (DrizzleTheme::panelBorder());
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());
}

void StatusBarComponent::resized()
{
    auto area = getLocalBounds().reduced (8, 4);
    auto header = area.removeFromTop (18);
    statsLabel.setBounds (header.removeFromRight (juce::jmin (header.getWidth(), 280)));
    statusDisplay.setBounds (area.reduced (0, 2));
}

//==============================================================================
SystemMetricsBar::SystemMetricsBar() {}

void SystemMetricsBar::paint (juce::Graphics& g)
{
    g.setColour (DrizzleTheme::textMuted());
    g.setFont (juce::FontOptions { 12.0f });
    g.drawText ("CPU 12%   48.0 kHz   Buffer 256 samples",
                getLocalBounds(),
                juce::Justification::centredRight);
}

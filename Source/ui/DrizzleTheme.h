#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace DrizzleTheme
{
inline juce::Colour background()        { return juce::Colour (0xff1a1d24); }
inline juce::Colour panelBackground()   { return juce::Colour (0xff22262f); }
inline juce::Colour panelBorder()       { return juce::Colour (0xff343a46); }
inline juce::Colour accent()            { return juce::Colour (0xff3d7eff); }
inline juce::Colour accentRed()         { return juce::Colour (0xffe53935); }
inline juce::Colour textPrimary()       { return juce::Colour (0xfff0f2f5); }
inline juce::Colour textMuted()         { return juce::Colour (0xff9aa3b2); }
inline juce::Colour meterGreen()        { return juce::Colour (0xff43a047); }
inline juce::Colour meterYellow()       { return juce::Colour (0xfffdd835); }
inline juce::Colour meterRed()          { return juce::Colour (0xffe53935); }

inline void applyLabel (juce::Label& label, bool muted = false)
{
    label.setColour (juce::Label::textColourId, muted ? textMuted() : textPrimary());
    label.setFont (juce::FontOptions { 13.0f });
}

inline void applyTrackToggleButton (juce::TextButton& button, bool isOn, bool isMuteButton = false)
{
    const auto offBg = panelBackground().brighter (0.08f);
    const auto onBg  = isMuteButton ? accentRed().withAlpha (0.85f) : accent().withAlpha (0.85f);
    const auto offText = textMuted();
    const auto onText  = textPrimary();

    button.setClickingTogglesState (true);
    button.setColour (juce::TextButton::buttonColourId, isOn ? onBg : offBg);
    button.setColour (juce::TextButton::buttonOnColourId, onBg);
    button.setColour (juce::TextButton::textColourOffId, isOn ? onText : offText);
    button.setColour (juce::TextButton::textColourOnId, onText);
}

inline void paintPanel (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title)
{
    g.setColour (panelBackground());
    g.fillRect (bounds);
    g.setColour (panelBorder());
    g.drawRect (bounds, 1);

    if (title.isNotEmpty())
    {
        auto header = bounds.removeFromTop (28);
        g.setColour (textMuted());
        g.setFont (juce::FontOptions { 12.0f }.withStyle ("Bold"));
        g.drawText (title, header.reduced (10, 0), juce::Justification::centredLeft);
    }
}
} // namespace DrizzleTheme

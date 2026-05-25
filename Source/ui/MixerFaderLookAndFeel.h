#pragma once

#include "DrizzleTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>

/** Vertical mixer fader: slot groove + rectangular cap. */
class MixerFaderLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider (juce::Graphics& g,
                           int x,
                           int y,
                           int width,
                           int height,
                           float sliderPos,
                           float minSliderPos,
                           float maxSliderPos,
                           juce::Slider::SliderStyle style,
                           juce::Slider& slider) override
    {
        juce::ignoreUnused (minSliderPos, maxSliderPos);

        if (style != juce::Slider::LinearVertical && style != juce::Slider::LinearBarVertical)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height,
                                              sliderPos, minSliderPos, maxSliderPos, style, slider);
            return;
        }

        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (3.0f, 4.0f);

        const float slotWidth = juce::jlimit (6.0f, 10.0f, bounds.getWidth() * 0.32f);
        auto slot = bounds.withSizeKeepingCentre (slotWidth, bounds.getHeight());

        g.setColour (DrizzleTheme::panelBackground().darker (0.45f));
        g.fillRoundedRectangle (slot, slotWidth * 0.5f);

        g.setColour (DrizzleTheme::panelBorder().withAlpha (0.9f));
        g.drawRoundedRectangle (slot, slotWidth * 0.5f, 1.0f);

        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRect (slot.getX() + slotWidth * 0.15f,
                    slot.getY() + 2.0f,
                    slotWidth * 0.2f,
                    slot.getHeight() - 4.0f);

        const float thumbWidth  = juce::jmin (bounds.getWidth() - 2.0f, slotWidth * 2.6f);
        const float thumbHeight = juce::jmax (18.0f, bounds.getWidth() * 1.35f);
        const float thumbCentreY = juce::jlimit (slot.getY() + thumbHeight * 0.5f,
                                                 slot.getBottom() - thumbHeight * 0.5f,
                                                 sliderPos);

        juce::Rectangle<float> thumb (bounds.getCentreX() - thumbWidth * 0.5f,
                                      thumbCentreY - thumbHeight * 0.5f,
                                      thumbWidth,
                                      thumbHeight);

        g.setColour (DrizzleTheme::panelBackground().brighter (0.12f));
        g.fillRoundedRectangle (thumb.expanded (1.0f, 1.0f), 4.0f);

        g.setColour (slider.isEnabled() ? DrizzleTheme::accent() : DrizzleTheme::textMuted());
        g.fillRoundedRectangle (thumb, 4.0f);

        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.fillRoundedRectangle (thumb.withTrimmedTop (thumb.getHeight() * 0.65f).reduced (2.0f, 1.0f), 2.0f);

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (thumb, 4.0f, 1.0f);
    }
};

inline void applyMixerFaderStyle (juce::Slider& slider, juce::LookAndFeel& laf)
{
    slider.setLookAndFeel (&laf);
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange (0.0, 1.0, 0.001);
    slider.setSkewFactorFromMidPoint (0.2);
    slider.setMouseDragSensitivity (180);
    slider.setDoubleClickReturnValue (true, 0.75);
    slider.setColour (juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::trackColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::thumbColourId, juce::Colours::transparentBlack);
}

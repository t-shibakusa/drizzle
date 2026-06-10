#pragma once

#include "DrizzleTheme.h"
#include "MixerDbScale.h"
#include <juce_gui_basics/juce_gui_basics.h>

/** Vertical mixer fader: dB scale, slot groove, rectangular cap. */
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

        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f, 4.0f);
        auto labelArea = bounds.removeFromRight (MixerDbScale::labelWidthPx);
        const auto faderArea = bounds;

        const float slotWidth = juce::jlimit (6.0f, 10.0f, faderArea.getWidth() * 0.32f);
        auto slot = faderArea.withSizeKeepingCentre (slotWidth, faderArea.getHeight());

        drawDbScale (g, slot, faderArea);

        g.setColour (DrizzleTheme::panelBackground().darker (0.45f));
        g.fillRoundedRectangle (slot, slotWidth * 0.5f);

        g.setColour (DrizzleTheme::panelBorder().withAlpha (0.9f));
        g.drawRoundedRectangle (slot, slotWidth * 0.5f, 1.0f);

        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRect (slot.getX() + slotWidth * 0.15f,
                    slot.getY() + 2.0f,
                    slotWidth * 0.2f,
                    slot.getHeight() - 4.0f);

        const float thumbWidth  = juce::jmin (faderArea.getWidth() - 2.0f, slotWidth * 2.6f) * (2.0f / 3.0f);
        const float thumbHeight = juce::jmax (18.0f, faderArea.getWidth() * 1.35f) * (1.0f / 3.0f);
        const float thumbRadius = juce::jmin (3.0f, thumbHeight * 0.28f);
        const float thumbCentreY = juce::jlimit (slot.getY() + thumbHeight * 0.5f,
                                                 slot.getBottom() - thumbHeight * 0.5f,
                                                 sliderPos);

        juce::Rectangle<float> thumb (slot.getCentreX() - thumbWidth * 0.5f,
                                      thumbCentreY - thumbHeight * 0.5f,
                                      thumbWidth,
                                      thumbHeight);

        g.setColour (DrizzleTheme::panelBackground().brighter (0.12f));
        g.fillRoundedRectangle (thumb.expanded (0.5f, 0.5f), thumbRadius);

        g.setColour (slider.isEnabled() ? DrizzleTheme::accent() : DrizzleTheme::textMuted());
        g.fillRoundedRectangle (thumb, thumbRadius);

        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.fillRoundedRectangle (thumb.withTrimmedTop (thumb.getHeight() * 0.65f).reduced (1.0f, 0.5f), thumbRadius * 0.5f);

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (thumb, thumbRadius, 1.0f);

        drawGainLabel (g, labelArea, thumbCentreY, (float) slider.getValue(), slider.isEnabled());
    }

private:
    static float dbToSlotY (float db, juce::Rectangle<float> slot)
    {
        const float t = MixerDbScale::normaliseDb (db);
        return slot.getBottom() - t * slot.getHeight();
    }

    static void drawDbScale (juce::Graphics& g, juce::Rectangle<float> slot, juce::Rectangle<float> faderArea)
    {
        const float scaleLeft = faderArea.getX() + 1.0f;
        const float scaleRight = slot.getX() - 2.0f;

        if (scaleRight <= scaleLeft + 2.0f)
            return;

        for (int db = (int) MixerDbScale::minDb; db <= (int) MixerDbScale::maxDb; db += 10)
        {
            const float y = dbToSlotY ((float) db, slot);
            const bool isZero = db == 0;
            const bool isEdge = db == (int) MixerDbScale::minDb || db == (int) MixerDbScale::maxDb;

            const float tickLen = isZero ? (scaleRight - scaleLeft) * 0.95f
                               : isEdge ? (scaleRight - scaleLeft) * 0.75f
                               :        (scaleRight - scaleLeft) * 0.55f;

            g.setColour (isZero ? DrizzleTheme::textPrimary().withAlpha (0.85f)
                                : DrizzleTheme::textMuted().withAlpha (isEdge ? 0.7f : 0.45f));
            g.drawLine (scaleRight - tickLen, y, scaleRight, y, isZero ? 1.5f : 1.0f);
        }

        const float zeroY = dbToSlotY (0.0f, slot);
        g.setColour (DrizzleTheme::accent().withAlpha (0.35f));
        g.drawLine (slot.getX(), zeroY, slot.getRight(), zeroY, 1.0f);
    }

    static void drawGainLabel (juce::Graphics& g,
                               juce::Rectangle<float> labelArea,
                               float thumbCentreY,
                               float gainDb,
                               bool enabled)
    {
        const float labelHeight = 11.0f;
        const float labelY = juce::jlimit (labelArea.getY(),
                                           labelArea.getBottom() - labelHeight,
                                           thumbCentreY - labelHeight * 0.5f);

        g.setFont (juce::FontOptions { 9.0f });
        g.setColour (enabled ? DrizzleTheme::textMuted() : DrizzleTheme::textMuted().withAlpha (0.45f));

        g.drawText (MixerDbScale::formatGainDb (gainDb),
                    labelArea.withY (labelY).withHeight (labelHeight),
                    juce::Justification::centredLeft,
                    false);
    }
};

inline void applyMixerFaderStyle (juce::Slider& slider, juce::LookAndFeel& laf)
{
    slider.setLookAndFeel (&laf);
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange (MixerDbScale::minDb, MixerDbScale::maxDb, 0.1);
    slider.setValue (MixerDbScale::defaultDb, juce::dontSendNotification);
    slider.setMouseDragSensitivity (180);
    slider.setDoubleClickReturnValue (true, MixerDbScale::defaultDb);
    slider.setColour (juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::trackColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::thumbColourId, juce::Colours::transparentBlack);
}

inline float readSnappedFaderDb (juce::Slider& slider)
{
    return MixerDbScale::applyZeroSnap ((float) slider.getValue());
}

inline void updateFaderWithZeroSnap (juce::Slider& slider, float gainDb)
{
    const float snapped = MixerDbScale::applyZeroSnap (gainDb);

    if (std::abs (snapped - (float) slider.getValue()) > 0.001f)
        slider.setValue (snapped, juce::dontSendNotification);
}

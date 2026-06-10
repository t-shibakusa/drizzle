#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

namespace MixerDbScale
{
inline constexpr float minDb       = -50.0f;
inline constexpr float maxDb       =  20.0f;
inline constexpr float defaultDb   =   0.0f;
inline constexpr float zeroSnapDb  =   1.0f;
inline constexpr float labelWidthPx  =  36.0f;

inline float dbToLinear (float db)
{
    return std::pow (10.0f, db * 0.05f);
}

inline float linearToDb (float linear)
{
    return 20.0f * std::log10 (juce::jmax (linear, 1.0e-6f));
}

inline float normaliseDb (float db)
{
    return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, 0.0f, 1.0f);
}

inline float dbFromNormalised (float normalised)
{
    return juce::jmap (juce::jlimit (0.0f, 1.0f, normalised), 0.0f, 1.0f, minDb, maxDb);
}

inline float applyZeroSnap (float db)
{
    if (std::abs (db) <= zeroSnapDb)
        return defaultDb;

    return db;
}

inline juce::String formatGainDb (float db)
{
    const float clamped = juce::jlimit (minDb, maxDb, db);
    const float rounded = std::round (clamped * 10.0f) / 10.0f;

    juce::String text;

    if (rounded > 0.05f)
        text = "+";

    text << juce::String (rounded, 1);
    return text;
}

/** Legacy session files stored linear gain 0..1. */
inline float migrateLegacyLinearGain (float storedGain)
{
    if (storedGain > 0.0f && storedGain <= 1.0f)
        return linearToDb (storedGain);

    return juce::jlimit (minDb, maxDb, storedGain);
}
} // namespace MixerDbScale

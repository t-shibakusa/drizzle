#pragma once

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>

enum class PluginPathCategory
{
    Vst = 0,
    Vst2,
    Vst3,
    Aax,
    count
};

/** Per-format plugin search directories (persisted). */
class PluginScanPaths
{
public:
    PluginScanPaths();

    static juce::String getCategoryDisplayName (PluginPathCategory category);
    static juce::String getCategoryXmlTag (PluginPathCategory category);

    const juce::StringArray& getPaths (PluginPathCategory category) const noexcept;
    juce::FileSearchPath getSearchPath (PluginPathCategory category) const;
    /** VST + VST2 directories combined (both use JUCE "VST" format). */
    juce::FileSearchPath getVst2CombinedSearchPath() const;

    bool addPath (PluginPathCategory category, const juce::File& directory);
    bool removePathAt (PluginPathCategory category, int index);

    bool hasAnyScannablePaths() const;

    void load();
    void save() const;
    void seedDefaultsIfEmpty();

private:
    static juce::File getPathsFile();
    static int categoryIndex (PluginPathCategory category) noexcept;

    void seedCategoryDefaults (PluginPathCategory category);

    std::array<juce::StringArray, (size_t) PluginPathCategory::count> pathsByCategory;
};

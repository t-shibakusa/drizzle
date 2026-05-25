#include "PluginScanPaths.h"

namespace
{
void addDirectoryIfExists (juce::StringArray& list, const juce::String& path)
{
    const juce::File directory (path);

    if (directory.isDirectory())
        list.addIfNotAlreadyThere (directory.getFullPathName());
}
} // namespace

PluginScanPaths::PluginScanPaths()
{
    load();
    seedDefaultsIfEmpty();
}

int PluginScanPaths::categoryIndex (PluginPathCategory category) noexcept
{
    return (int) category;
}

juce::String PluginScanPaths::getCategoryDisplayName (PluginPathCategory category)
{
    switch (category)
    {
        case PluginPathCategory::Vst:  return "VST";
        case PluginPathCategory::Vst2: return "VST2";
        case PluginPathCategory::Vst3: return "VST3";
        case PluginPathCategory::Aax:  return "AAX";
        case PluginPathCategory::count: break;
    }

    return {};
}

juce::String PluginScanPaths::getCategoryXmlTag (PluginPathCategory category)
{
    return getCategoryDisplayName (category);
}

const juce::StringArray& PluginScanPaths::getPaths (PluginPathCategory category) const noexcept
{
    return pathsByCategory[(size_t) categoryIndex (category)];
}

juce::File PluginScanPaths::getPathsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Drizzle")
               .getChildFile ("plugin_paths.xml");
}

void PluginScanPaths::load()
{
    for (auto& paths : pathsByCategory)
        paths.clear();

    const auto file = getPathsFile();

    if (! file.existsAsFile())
        return;

    if (auto xml = juce::XmlDocument::parse (file))
    {
        bool loadedCategorized = false;

        for (int i = 0; i < (int) PluginPathCategory::count; ++i)
        {
            const auto category = (PluginPathCategory) i;
            const auto tag = getCategoryXmlTag (category);

            if (auto* section = xml->getChildByName (tag))
            {
                loadedCategorized = true;

                for (auto* pathNode : section->getChildIterator())
                    if (pathNode->hasTagName ("PATH"))
                    {
                        const auto path = pathNode->getAllSubText().trim();

                        if (juce::File (path).isDirectory())
                            pathsByCategory[(size_t) i].addIfNotAlreadyThere (path);
                    }
            }
        }

        if (! loadedCategorized)
        {
            for (auto* child : xml->getChildIterator())
            {
                if (! child->hasTagName ("PATH"))
                    continue;

                const auto path = child->getAllSubText().trim();

                if (! juce::File (path).isDirectory())
                    continue;

                const auto lower = path.toLowerCase();

                if (lower.contains ("vst3") || lower.contains ("common files\\vst3"))
                    pathsByCategory[(size_t) PluginPathCategory::Vst3].addIfNotAlreadyThere (path);
                else if (lower.contains ("avid") || lower.contains ("aax"))
                    pathsByCategory[(size_t) PluginPathCategory::Aax].addIfNotAlreadyThere (path);
                else if (lower.contains ("(x86)") || lower.contains ("x86\\"))
                    pathsByCategory[(size_t) PluginPathCategory::Vst].addIfNotAlreadyThere (path);
                else
                    pathsByCategory[(size_t) PluginPathCategory::Vst2].addIfNotAlreadyThere (path);
            }
        }
    }
}

void PluginScanPaths::save() const
{
    juce::XmlElement root ("PLUGIN_PATHS");

    for (int i = 0; i < (int) PluginPathCategory::count; ++i)
    {
        const auto category = (PluginPathCategory) i;
        auto* section = root.createNewChildElement (getCategoryXmlTag (category));

        for (const auto& path : pathsByCategory[(size_t) i])
            section->createNewChildElement ("PATH")->addTextElement (path);
    }

    const auto file = getPathsFile();
    file.getParentDirectory().createDirectory();
    root.writeTo (file, {});
}

juce::FileSearchPath PluginScanPaths::getSearchPath (PluginPathCategory category) const
{
    juce::FileSearchPath result;

    for (const auto& path : getPaths (category))
        if (juce::File (path).isDirectory())
            result.add (juce::File (path));

    return result;
}

juce::FileSearchPath PluginScanPaths::getVst2CombinedSearchPath() const
{
    auto result = getSearchPath (PluginPathCategory::Vst);

    for (int i = 0; i < getSearchPath (PluginPathCategory::Vst2).getNumPaths(); ++i)
        result.addIfNotAlreadyThere (getSearchPath (PluginPathCategory::Vst2)[i]);

    result.removeRedundantPaths();
    return result;
}

bool PluginScanPaths::addPath (PluginPathCategory category, const juce::File& directory)
{
    if (! directory.isDirectory())
        return false;

    const auto path = directory.getFullPathName();
    auto& list = pathsByCategory[(size_t) categoryIndex (category)];

    if (list.contains (path))
        return false;

    list.add (path);
    save();
    return true;
}

bool PluginScanPaths::removePathAt (PluginPathCategory category, int index)
{
    auto& list = pathsByCategory[(size_t) categoryIndex (category)];

    if (! juce::isPositiveAndBelow (index, list.size()))
        return false;

    list.remove (index);
    save();
    return true;
}

bool PluginScanPaths::hasAnyScannablePaths() const
{
    return getSearchPath (PluginPathCategory::Vst3).getNumPaths() > 0
        || getVst2CombinedSearchPath().getNumPaths() > 0;
}

void PluginScanPaths::seedCategoryDefaults (PluginPathCategory category)
{
    auto& list = pathsByCategory[(size_t) categoryIndex (category)];

    switch (category)
    {
        case PluginPathCategory::Vst:
            addDirectoryIfExists (list, "C:\\Program Files (x86)\\VstPlugins");
            addDirectoryIfExists (list, "C:\\Program Files (x86)\\Steinberg\\VstPlugins");
            break;

        case PluginPathCategory::Vst2:
            addDirectoryIfExists (list, "C:\\Program Files\\VSTPlugins");
            addDirectoryIfExists (list, "C:\\Program Files\\Steinberg\\VstPlugins");

            if (const juce::File wavesDir ("C:\\Program Files (x86)\\Waves"); wavesDir.isDirectory())
            {
                for (const auto& shellDir : wavesDir.findChildFiles (juce::File::findDirectories, false, "WaveShells*"))
                    addDirectoryIfExists (list, shellDir.getFullPathName());
            }
            break;

        case PluginPathCategory::Vst3:
            addDirectoryIfExists (list, "C:\\Program Files\\Common Files\\VST3");
            addDirectoryIfExists (list, juce::File::getSpecialLocation (juce::File::windowsLocalAppData)
                                              .getChildFile ("Programs")
                                              .getChildFile ("Common")
                                              .getChildFile ("VST3")
                                              .getFullPathName());
            break;

        case PluginPathCategory::Aax:
            addDirectoryIfExists (list, "C:\\Program Files\\Common Files\\Avid\\Audio\\Plug-Ins");
            addDirectoryIfExists (list, "C:\\Program Files (x86)\\Common Files\\Avid\\Audio\\Plug-Ins");
            break;

        case PluginPathCategory::count:
            break;
    }
}

void PluginScanPaths::seedDefaultsIfEmpty()
{
    bool anySaved = false;

    for (const auto& paths : pathsByCategory)
        if (paths.size() > 0)
            anySaved = true;

    if (anySaved)
        return;

    for (int i = 0; i < (int) PluginPathCategory::count; ++i)
        seedCategoryDefaults ((PluginPathCategory) i);

    save();
}

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace ApplicationShutdown
{
inline void dismissModalComponents()
{
    if (auto* mcm = juce::ModalComponentManager::getInstanceWithoutCreating())
        mcm->cancelAllModalComponents();
}

inline void closeExtraTopLevelWindows (juce::Component* mainWindowComponent)
{
    for (int i = juce::TopLevelWindow::getNumTopLevelWindows(); --i >= 0;)
    {
        if (auto* window = juce::TopLevelWindow::getTopLevelWindow (i))
        {
            if (window == mainWindowComponent || mainWindowComponent->isParentOf (window))
                continue;

            window->setVisible (false);
            delete window;
        }
    }
}
} // namespace ApplicationShutdown

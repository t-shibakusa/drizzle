#include <juce_gui_extra/juce_gui_extra.h>
#include "ApplicationShutdown.h"
#include "MainComponent.h"
#include "SessionSettings.h"
#include "Vst3HostIdentity.h"
#include "ui/DrizzleTheme.h"

class DrizzleApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return "Drizzle"; }
    const juce::String getApplicationVersion() override    { return "0.1.0"; }

    void initialise (const juce::String& commandLine) override
    {
        if (DrizzleVst3Host::isLicenseCompatProcess())
            DrizzleVst3Host::setHostApplicationName ("Reaper");
        else if (DrizzleVst3Host::shouldAutoLaunchLicenseCompat (commandLine))
        {
            if (DrizzleVst3Host::launchLicenseCompatAndQuit (commandLine))
                return;
        }

        DrizzleVst3Host::writeLicenseCompatDiagnostic();
        mainWindow.reset (new MainWindow (*this));
    }

    void shutdown() override
    {
        if (mainWindow != nullptr)
            mainWindow->prepareForShutdown();

        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr)
            mainWindow->prepareForShutdown();

        JUCEApplication::systemRequestedQuit();
    }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (JUCEApplication& app)
            : DocumentWindow (DrizzleVst3Host::isLicenseCompatProcess()
                                  ? juce::String::fromUTF8 (u8"Drizzle (\u30e9\u30a4\u30bb\u30f3\u30xb9\u4e92\u63db: reaper.exe)")
                                  : "Drizzle",
                              DrizzleTheme::background(),
                              DocumentWindow::allButtons),
              application (app)
        {
            setUsingNativeTitleBar (true);
            setResizable (true, true);
            setContentOwned (new MainComponent(), true);

            if (auto* mc = dynamic_cast<MainComponent*> (getContentComponent()))
                setMenuBar (mc);

           #if JUCE_WINDOWS
            setTitleBarHeight (30);
           #endif

            const auto settings = SessionSettingsStore::load();

            if (settings.window.hasSavedBounds)
                setBounds (settings.window.x,
                           settings.window.y,
                           settings.window.width,
                           settings.window.height);
            else
                centreWithSize (settings.window.width, settings.window.height);

            setVisible (true);
        }

        void moved() override
        {
            DocumentWindow::moved();
            persistWindowBounds();
        }

        void resized() override
        {
            DocumentWindow::resized();
            persistWindowBounds();
        }

        void closeButtonPressed() override
        {
            application.systemRequestedQuit();
        }

        void prepareForShutdown()
        {
            if (shutdownPrepared)
                return;

            shutdownPrepared = true;

            persistWindowBounds();
            setMenuBar (nullptr);

            if (auto* mc = dynamic_cast<MainComponent*> (getContentComponent()))
                mc->prepareForShutdown();

            ApplicationShutdown::dismissModalComponents();
            ApplicationShutdown::closeExtraTopLevelWindows (this);
            clearContentComponent();
        }

    private:
        void persistWindowBounds()
        {
            if (! isShowing())
                return;

            const auto b = getBounds();
            SessionSettingsStore::saveWindowBounds (b.getX(), b.getY(), b.getWidth(), b.getHeight());
        }

        JUCEApplication& application;
        bool shutdownPrepared = false;
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (DrizzleApplication)

#include "Vst3HostIdentity.h"
#include <juce_events/juce_events.h>
#include <atomic>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <shellapi.h>
#endif

namespace
{
juce::String currentHostName { "Reaper" };
std::atomic<bool> xlnOiLaunchedThisSession { false };

bool isXlnManufacturer (const juce::String& manufacturer)
{
    return manufacturer.containsIgnoreCase ("XLN");
}

} // namespace

namespace DrizzleVst3Host
{
juce::String getHostApplicationName()
{
    return currentHostName;
}

void setHostApplicationName (const juce::String& name)
{
    const auto trimmed = name.trim();
    currentHostName = trimmed.isNotEmpty() ? trimmed : "Reaper";
}

bool isLicenseCompatProcess()
{
    return juce::File::getSpecialLocation (juce::File::currentApplicationFile)
               .getFileName()
               .equalsIgnoreCase ("reaper.exe");
}

juce::File getReaperInstallDirectory()
{
#if JUCE_WINDOWS
    const auto path = juce::WindowsRegistry::getValue ("HKEY_LOCAL_MACHINE\\Software\\REAPER");

    if (path.isNotEmpty())
        return juce::File (path);
#endif

    return juce::File ("C:\\Program Files\\REAPER (x64)");
}

juce::File getLicenseCompatDirectory()
{
    const auto appFile = juce::File::getSpecialLocation (juce::File::currentApplicationFile);

    if (appFile.getParentDirectory().getFileName().equalsIgnoreCase ("LicenseCompat"))
        return appFile.getParentDirectory();

    if (appFile.getParentDirectory().getFileName().equalsIgnoreCase ("DrizzleLicenseHost"))
        return appFile.getParentDirectory();

    return appFile.getParentDirectory().getChildFile ("LicenseCompat");
}

juce::File getBundledLicenseCompatExecutable()
{
    return getLicenseCompatDirectory().getChildFile ("reaper.exe");
}

juce::File getInstalledLicenseCompatExecutable()
{
    return getReaperInstallDirectory()
               .getChildFile ("DrizzleLicenseHost")
               .getChildFile ("reaper.exe");
}

juce::File getLicenseCompatReaperExecutable()
{
    const auto installed = getInstalledLicenseCompatExecutable();

    if (installed.existsAsFile())
        return installed;

    return getBundledLicenseCompatExecutable();
}

bool shouldAutoLaunchLicenseCompat (const juce::String& commandLine)
{
    if (isLicenseCompatProcess())
        return false;

    return ! commandLine.containsIgnoreCase ("--no-license-compat");
}

bool launchLicenseCompatAndQuit (const juce::String& forwardedCommandLine)
{
    const auto compatExe = getLicenseCompatReaperExecutable();

    if (! compatExe.existsAsFile())
        return false;

    juce::String launchParameters;

    for (const auto& token : juce::StringArray::fromTokens (forwardedCommandLine, " ", "\""))
    {
        const auto trimmed = token.trim();

        if (trimmed.isEmpty() || trimmed.equalsIgnoreCase ("--no-license-compat"))
            continue;

        launchParameters << (launchParameters.isEmpty() ? "" : " ") << trimmed;
    }

    if (! compatExe.startAsProcess (launchParameters))
        return false;

    if (auto* app = juce::JUCEApplicationBase::getInstance())
        app->systemRequestedQuit();

    return true;
}

bool installLicenseCompatToReaperFolder()
{
    const auto script = getLicenseCompatDirectory().getChildFile ("install_to_reaper_folder.bat");

    if (! script.existsAsFile())
        return false;

#if JUCE_WINDOWS
    const auto scriptPath = script.getFullPathName();
    const auto result = ShellExecuteW (nullptr,
                                       L"runas",
                                       scriptPath.toWideCharPointer(),
                                       nullptr,
                                       script.getParentDirectory().getFullPathName().toWideCharPointer(),
                                       SW_SHOWNORMAL);
    return (INT_PTR) result > 32;
#else
    return script.startAsProcess();
#endif
}

void ensureXlnOnlineInstallerRunning()
{
    if (! isLicenseCompatProcess())
        return;

    if (xlnOiLaunchedThisSession.exchange (true))
        return;

    const juce::File installer ("C:\\Program Files\\XLN Audio\\XLN Online Installer\\XLN Online Installer.exe");

    if (installer.existsAsFile())
        installer.startAsProcess();
}

void prepareForLicensedPluginLoad (const juce::String& manufacturerName)
{
    if (isXlnManufacturer (manufacturerName))
        ensureXlnOnlineInstallerRunning();
}

void writeLicenseCompatDiagnostic()
{
    const auto logFile = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                             .getChildFile ("Drizzle")
                             .getChildFile ("license_compat.log");

    logFile.getParentDirectory().createDirectory();

    const auto appFile = juce::File::getSpecialLocation (juce::File::currentApplicationFile);

    juce::String log;
    log << "Time: " << juce::Time::getCurrentTime().toString (true, true) << "\n";
    log << "ExePath: " << appFile.getFullPathName() << "\n";
    log << "ExeName: " << appFile.getFileName() << "\n";
    log << "CompatMode: " << (isLicenseCompatProcess() ? "yes" : "no") << "\n";
    log << "HostName: " << getHostApplicationName() << "\n";
    log << "BundledCompat: " << getBundledLicenseCompatExecutable().getFullPathName() << "\n";
    log << "InstalledCompat: " << getInstalledLicenseCompatExecutable().getFullPathName() << "\n";
    log << "LaunchTarget: " << getLicenseCompatReaperExecutable().getFullPathName() << "\n";
    log << "ReaperDir: " << getReaperInstallDirectory().getFullPathName() << "\n";

    logFile.replaceWithText (log);
}
} // namespace DrizzleVst3Host

#if DRIZZLE_HAS_VST3_HOST_NAME_HOOK
namespace juce
{
juce::String drizzle_getVst3HostApplicationName()
{
    return DrizzleVst3Host::getHostApplicationName();
}
} // namespace juce
#endif

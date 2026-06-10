#pragma once

#include <juce_core/juce_core.h>

namespace DrizzleVst3Host
{
    juce::String getHostApplicationName();
    void setHostApplicationName (const juce::String& name);

    /** True when launched as LicenseCompat/reaper.exe (XLN / iZotope license ID). */
    bool isLicenseCompatProcess();

    juce::File getReaperInstallDirectory();
    juce::File getLicenseCompatDirectory();
    juce::File getBundledLicenseCompatExecutable();
    juce::File getInstalledLicenseCompatExecutable();

    /** Prefers Reaper-folder install, then bundled LicenseCompat/reaper.exe. */
    juce::File getLicenseCompatReaperExecutable();

    /** Skip auto-restart when Drizzle.exe is launched with --no-license-compat. */
    bool shouldAutoLaunchLicenseCompat (const juce::String& commandLine);

    /** Launch LicenseCompat/reaper.exe and quit the current process. */
    bool launchLicenseCompatAndQuit (const juce::String& forwardedCommandLine = {});

    /** Copy bundled reaper.exe into %ProgramFiles%\\REAPER (x64)\\DrizzleLicenseHost (admin). */
    bool installLicenseCompatToReaperFolder();

    /** Start XLN Online Installer once per session before loading XLN plugins. */
    void ensureXlnOnlineInstallerRunning();

    void prepareForLicensedPluginLoad (const juce::String& manufacturerName);

    /** Write %AppData%\\Drizzle\\license_compat.log for troubleshooting. */
    void writeLicenseCompatDiagnostic();
}

#if DRIZZLE_HAS_VST3_HOST_NAME_HOOK
namespace juce
{
juce::String drizzle_getVst3HostApplicationName();
}
#endif

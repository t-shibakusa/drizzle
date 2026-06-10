#include "StreamEngine.h"
#include <vector>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

#if JUCE_WINDOWS
struct FfmpegProcess
{
    bool start (const juce::String& command, const juce::String& workingDirectory = {})
    {
        kill();

        SECURITY_ATTRIBUTES securityAttributes {};
        securityAttributes.nLength = sizeof (securityAttributes);
        securityAttributes.bInheritHandle = TRUE;

        HANDLE stdinRead = nullptr;
        HANDLE stdinWrite = nullptr;
        HANDLE stderrRead = nullptr;
        HANDLE stderrWrite = nullptr;

        if (! CreatePipe (&stdinRead, &stdinWrite, &securityAttributes, 0))
            return false;

        if (! CreatePipe (&stderrRead, &stderrWrite, &securityAttributes, 0))
        {
            CloseHandle (stdinRead);
            CloseHandle (stdinWrite);
            return false;
        }

        SetHandleInformation (stdinWrite, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation (stderrRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW startupInfo {};
        startupInfo.cb = sizeof (startupInfo);
        startupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        startupInfo.wShowWindow = SW_HIDE;
        startupInfo.hStdInput = stdinRead;
        startupInfo.hStdOutput = GetStdHandle (STD_OUTPUT_HANDLE);
        startupInfo.hStdError = stderrWrite;

        PROCESS_INFORMATION processInfo {};
        std::vector<wchar_t> commandLine ((size_t) command.length() + 2, L'\0');
        command.copyToUTF16 (reinterpret_cast<juce::CharPointer_UTF16::CharType*> (commandLine.data()),
                             commandLine.size() * sizeof (wchar_t));

        std::vector<wchar_t> workingDirectoryUtf16;
        const wchar_t* workingDirectoryPtr = nullptr;

        if (workingDirectory.isNotEmpty())
        {
            workingDirectoryUtf16.assign ((size_t) workingDirectory.length() + 1, L'\0');
            workingDirectory.copyToUTF16 (reinterpret_cast<juce::CharPointer_UTF16::CharType*> (workingDirectoryUtf16.data()),
                                        workingDirectoryUtf16.size() * sizeof (wchar_t));
            workingDirectoryPtr = workingDirectoryUtf16.data();
        }

        const auto created = CreateProcessW (nullptr,
                                             commandLine.data(),
                                             nullptr,
                                             nullptr,
                                             TRUE,
                                             CREATE_NO_WINDOW,
                                             nullptr,
                                             workingDirectoryPtr,
                                             &startupInfo,
                                             &processInfo);

        CloseHandle (stdinRead);
        CloseHandle (stderrWrite);

        if (! created)
        {
            CloseHandle (stdinWrite);
            CloseHandle (stderrRead);
            return false;
        }

        CloseHandle (processInfo.hThread);
        processHandle = processInfo.hProcess;
        stdinPipe = stdinWrite;
        stderrPipe = stderrRead;
        return true;
    }

    bool write (const void* data, int numBytes)
    {
        if (stdinPipe == nullptr || data == nullptr || numBytes <= 0)
            return false;

        DWORD bytesWritten = 0;
        return WriteFile (stdinPipe, data, (DWORD) numBytes, &bytesWritten, nullptr) != FALSE;
    }

    bool isRunning() const
    {
        if (processHandle == nullptr)
            return false;

        DWORD exitCode = STILL_ACTIVE;
        GetExitCodeProcess (processHandle, &exitCode);
        return exitCode == STILL_ACTIVE;
    }

    uint32_t getProcessId() const
    {
        return processHandle != nullptr ? (uint32_t) GetProcessId (processHandle) : 0u;
    }

    juce::String readStderr()
    {
        if (stderrPipe == nullptr)
            return {};

        juce::MemoryBlock buffer;
        char chunk[512];

        while (true)
        {
            DWORD available = 0;

            if (! PeekNamedPipe (stderrPipe, nullptr, 0, nullptr, &available, nullptr) || available == 0)
                break;

            const DWORD toRead = juce::jmin<DWORD> (available, (DWORD) sizeof (chunk));
            DWORD bytesRead = 0;

            if (! ReadFile (stderrPipe, chunk, toRead, &bytesRead, nullptr) || bytesRead == 0)
                break;

            buffer.append (chunk, (size_t) bytesRead);
        }

        return buffer.toString();
    }

    void kill()
    {
        if (stdinPipe != nullptr)
        {
            CloseHandle (stdinPipe);
            stdinPipe = nullptr;
        }

        if (processHandle != nullptr)
        {
            if (isRunning())
            {
                WaitForSingleObject (processHandle, 5000);

                if (isRunning())
                    TerminateProcess (processHandle, 1);

                WaitForSingleObject (processHandle, 2000);
            }

            CloseHandle (processHandle);
            processHandle = nullptr;
        }

        if (stderrPipe != nullptr)
        {
            CloseHandle (stderrPipe);
            stderrPipe = nullptr;
        }
    }

    ~FfmpegProcess()
    {
        kill();
    }

private:
    HANDLE processHandle = nullptr;
    HANDLE stdinPipe = nullptr;
    HANDLE stderrPipe = nullptr;
};
#else
struct FfmpegProcess
{
    bool start (const juce::String&, const juce::String& = {}) { return false; }
    bool write (const void*, int) { return false; }
    bool isRunning() const { return false; }
    uint32_t getProcessId() const { return 0u; }
    juce::String readStderr() { return {}; }
    void kill() {}
};
#endif

namespace
{
constexpr int kFfmpegChunkFrames = 1024;

juce::File getDrizzleStreamRuntimeDir()
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getChildFile ("DrizzleStream");
}

juce::File getActiveFfmpegPidFile()
{
    return getDrizzleStreamRuntimeDir().getChildFile ("active_ffmpeg.pid");
}

#if JUCE_WINDOWS
void terminateStaleFfmpegFromPidFile()
{
    const auto pidFile = getActiveFfmpegPidFile();

    if (! pidFile.existsAsFile())
        return;

    const auto pidText = pidFile.loadFileAsString().trim();
    const auto pid = (DWORD) pidText.getIntValue();

    if (pid == 0)
    {
        pidFile.deleteFile();
        return;
    }

    HANDLE processHandle = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);

    if (processHandle != nullptr)
    {
        DWORD size = 1024;
        std::vector<wchar_t> imagePath ((size_t) size + 1, L'\0');

        if (QueryFullProcessImageNameW (processHandle, 0, imagePath.data(), &size))
        {
            const juce::String fullPath (imagePath.data());
            const auto processName = juce::File (fullPath).getFileName();

            if (processName.equalsIgnoreCase ("ffmpeg.exe"))
            {
                TerminateProcess (processHandle, 1);
                WaitForSingleObject (processHandle, 3000);
            }
        }

        CloseHandle (processHandle);
    }

    pidFile.deleteFile();
}
#else
void terminateStaleFfmpegFromPidFile() {}
#endif

void writeActiveFfmpegPid (uint32_t pid)
{
    if (pid == 0u)
        return;

    const auto runtimeDir = getDrizzleStreamRuntimeDir();
    runtimeDir.createDirectory();
    getActiveFfmpegPidFile().replaceWithText (juce::String ((int) pid));
}

void clearActiveFfmpegPid()
{
    getActiveFfmpegPidFile().deleteFile();
}

juce::String escapeDrawtextText (const juce::String& text)
{
    juce::String escaped;
    escaped.preallocateBytes (text.length() * 2);

    for (auto c : text)
    {
        if (c == '\\' || c == '\'' || c == ':' || c == '%')
            escaped += '\\';

        escaped += juce::String::charToString (c);
    }

    return escaped;
}

juce::String stateToStatusText (StreamState state, const juce::String& error)
{
    switch (state)
    {
        case StreamState::idle:      return juce::String::fromUTF8 (u8"\u6e96\u5099\u5b8c\u4e86");
        case StreamState::starting:  return juce::String::fromUTF8 (u8"\u914d\u4fe1\u958b\u59cb\u4e2d...");
        case StreamState::live:      return juce::String::fromUTF8 (u8"\u914d\u4fe1\u4e2d");
        case StreamState::stopping:  return juce::String::fromUTF8 (u8"\u914d\u4fe1\u7d42\u4e86\u4e2d...");
        case StreamState::error:     return error.isNotEmpty() ? error
                                                               : juce::String::fromUTF8 (u8"\u30a8\u30e9\u30fc");
        default:                     return {};
    }
}

juce::File findExecutableInPath (const juce::String& executableName)
{
    const auto pathEnv = juce::SystemStats::getEnvironmentVariable ("PATH", "");
    juce::StringArray directories;
    directories.addTokens (pathEnv, ";", "");
    directories.removeEmptyStrings (true);

    for (const auto& directory : directories)
    {
        const juce::File candidate = juce::File (directory).getChildFile (executableName);

        if (candidate.existsAsFile())
            return candidate;
    }

    return {};
}

class StreamWorkerThread final : public juce::Thread
{
public:
    explicit StreamWorkerThread (StreamEngine& engineIn)
        : juce::Thread ("Drizzle Stream"),
          engine (engineIn)
    {
    }

    void run() override
    {
        engine.runStreamThread();
    }

private:
    StreamEngine& engine;
};
} // namespace

StreamEngine::StreamEngine() = default;

StreamEngine::~StreamEngine()
{
    stopStream();
    terminateStaleFfmpegFromPidFile();
}

void StreamEngine::setSampleRate (double sampleRateIn) noexcept
{
    sampleRate.store (sampleRateIn > 0.0 ? sampleRateIn : 44100.0);
}

void StreamEngine::pushAudio (const juce::AudioBuffer<float>& stereoBuffer, int numSamples)
{
    const auto currentState = state.load();

    if (currentState != StreamState::live && currentState != StreamState::starting)
        return;

    if (stereoBuffer.getNumChannels() < 1 || numSamples <= 0)
        return;

    const auto* left  = stereoBuffer.getReadPointer (0);
    const auto* right = stereoBuffer.getNumChannels() > 1 ? stereoBuffer.getReadPointer (1) : left;

    float peak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
        peak = juce::jmax (peak, std::abs (left[i]), std::abs (right[i]));

    outputPeakLevel.store (juce::jlimit (0.0f, 1.0f, peak * 2.5f));
    audioFifo.pushStereo (left, right, numSamples);
}

void StreamEngine::setConfig (const StreamConfig& newConfig)
{
    const juce::ScopedLock lock (configLock);
    config = newConfig;
    config.streamKey = normalizeYoutubeStreamKey (config.streamKey);
}

StreamConfig StreamEngine::getConfig() const
{
    const juce::ScopedLock lock (configLock);
    return config;
}

juce::File StreamEngine::findFfmpegExecutable()
{
    if (const auto besideApp = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                                    .getParentDirectory()
                                    .getChildFile ("ffmpeg.exe");
        besideApp.existsAsFile())
        return besideApp;

    if (const auto fromPath = findExecutableInPath ("ffmpeg.exe");
        fromPath.existsAsFile())
        return fromPath;

    const juce::StringArray candidates {
        "C:\\ffmpeg\\bin\\ffmpeg.exe",
        "C:\\Program Files\\ffmpeg\\bin\\ffmpeg.exe",
        "C:\\tools\\ffmpeg\\bin\\ffmpeg.exe"
    };

    for (const auto& path : candidates)
        if (juce::File (path).existsAsFile())
            return juce::File (path);

    return {};
}

juce::String StreamEngine::normalizeYoutubeStreamKey (const juce::String& input)
{
    auto text = input.trim();

    if (text.isEmpty())
        return {};

    if (text.startsWithIgnoreCase ("rtmp://") || text.startsWithIgnoreCase ("rtmps://"))
    {
        const auto lower = text.toLowerCase();
        const int live2Slash = lower.indexOf ("/live2/");

        if (live2Slash >= 0)
        {
            const auto key = text.substring (live2Slash + 7).trim();
            return key;
        }

        if (lower.endsWith ("/live2") || lower.endsWith ("/live2/"))
            return {};

        return text;
    }

    if (text.startsWithIgnoreCase ("live2/"))
        text = text.substring (6).trim();

    return text;
}

juce::String StreamEngine::getYoutubeRtmpUrl (const juce::String& streamKey)
{
    const auto key = normalizeYoutubeStreamKey (streamKey);

    if (key.isEmpty())
        return {};

    if (key.startsWithIgnoreCase ("rtmp://") || key.startsWithIgnoreCase ("rtmps://"))
        return key;

    return "rtmp://a.rtmp.youtube.com/live2/" + key;
}

juce::File StreamEngine::findTitleFontSource()
{
    const juce::StringArray candidates {
        "C:\\Windows\\Fonts\\meiryo.ttc",
        "C:\\Windows\\Fonts\\meiryob.ttc",
        "C:\\Windows\\Fonts\\msgothic.ttc",
        "C:\\Windows\\Fonts\\arial.ttf"
    };

    for (const auto& path : candidates)
        if (juce::File (path).existsAsFile())
            return juce::File (path);

    return {};
}

bool StreamEngine::prepareStreamAssets (const juce::String& title)
{
    juce::ignoreUnused (title);

    streamWorkDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("DrizzleStream")
                        .getChildFile (juce::Uuid().toString());

    if (! streamWorkDir.createDirectory())
        return false;

    streamFontFile = streamWorkDir.getChildFile ("drizzle_meiryo.ttc");

    const auto fontSource = findTitleFontSource();

    if (fontSource.existsAsFile())
        return fontSource.copyFileTo (streamFontFile);

    streamFontFile = {};
    return true;
}

juce::String StreamEngine::buildFfmpegCommand (const juce::File& ffmpeg,
                                               const juce::String& streamKey,
                                               const juce::String& title,
                                               const bool hasFont) const
{
    const auto inputSr = juce::String (juce::roundToInt (sampleRate.load()));
    const auto titleText = title.isNotEmpty()
                               ? title
                               : juce::String::fromUTF8 (u8"Drizzle Live");
    const auto escapedTitle = escapeDrawtextText (titleText);
    const auto titleDrawtext = juce::String ("drawtext=fontfile=drizzle_meiryo.ttc")
                           + ":text='" + escapedTitle + "'"
                           + ":fontsize=42:fontcolor=white"
                           + ":x=(w-text_w)/2:y=(h-text_h)/2";
    const auto clockDrawtext = juce::String (",drawtext=fontfile=drizzle_meiryo.ttc")
                             + ":text='%{pts\\:hms}'"
                             + ":fontsize=22:fontcolor=white@0.6"
                             + ":x=12:y=12"
                             + ",noise=c0s=1:c0f=t+u";
    const auto videoFilter = hasFont
                                 ? titleDrawtext + clockDrawtext
                                 : juce::String ("noise=c0s=2:c0f=t+u");

    return "\"" + ffmpeg.getFullPathName() + "\""
         + " -loglevel warning -nostats"
         + " -f lavfi -i color=c=0x2b3140:s=1280x720:r=30"
         + " -re -f s16le -ar " + inputSr + " -ac 2 -thread_queue_size 4096 -i pipe:0"
         + " -vf \"" + videoFilter + "\""
         + " -map 0:v:0 -map 1:a:0"
         + " -c:v libx264 -preset veryfast -tune zerolatency -profile:v main"
         + " -pix_fmt yuv420p -r 30 -fps_mode cfr -g 30 -keyint_min 30 -sc_threshold 0 -bf 0"
         + " -b:v 2500k -minrate 2500k -maxrate 2500k -bufsize 2500k"
         + " -x264-params nal-hrd=cbr:force-cfr=1"
         + " -c:a aac -b:a 160k -ar 48000 -ac 2"
         + " -max_muxing_queue_size 1024 -muxdelay 0 -muxpreload 0"
         + " -flvflags no_duration_filesize"
         + " -f flv \"" + getYoutubeRtmpUrl (streamKey) + "\"";
}

void StreamEngine::setState (StreamState newState, const juce::String& error)
{
    state.store (newState);

    if (error.isNotEmpty())
        lastError = error;

    if (newState == StreamState::live)
        liveStartMs.store ((int64_t) juce::Time::getMillisecondCounterHiRes());
    else if (newState == StreamState::idle)
        liveStartMs.store (0);

    sendChangeMessage();
}

bool StreamEngine::testConnection()
{
    const auto ffmpeg = findFfmpegExecutable();

    if (! ffmpeg.existsAsFile())
    {
        setState (StreamState::error,
                  juce::String::fromUTF8 (u8"FFmpeg \u304c\u898b\u3064\u304b\u308a\u307e\u305b\u3093\u3002"
                                          u8"ffmpeg.exe \u3092 PATH \u307e\u305f\u306f Drizzle.exe \u3068\u540c\u3058\u30d5\u30a9\u30eb\u30c0\u306b\u914d\u7f6e\u3057\u3066\u304f\u3060\u3055\u3044\u3002"));
        return false;
    }

    const auto cfg = getConfig();

    if (cfg.streamKey.isEmpty())
    {
        setState (StreamState::error,
                  juce::String::fromUTF8 (u8"\u30b9\u30c8\u30ea\u30fc\u30e0\u30ad\u30fc\u304c\u672a\u8a2d\u5b9a\u3067\u3059\u3002"
                                          u8"YouTube Studio \u306e\u30ad\u30fc\u6587\u5b57\u5217\u306e\u307f\u3092\u5165\u529b\u3057\u3066\u304f\u3060\u3055\u3044\u3002"
                                          u8"rtmp://... \u306f\u4e0d\u8981\u3067\u3059\u3002"));
        return false;
    }

    juce::ChildProcess probe;
    const auto command = "\"" + ffmpeg.getFullPathName() + "\" -version";

    if (! probe.start (command))
    {
        setState (StreamState::error,
                  juce::String::fromUTF8 (u8"FFmpeg \u306e\u8d77\u52d5\u306b\u5931\u6557\u3057\u307e\u3057\u305f\u3002"));
        return false;
    }

    probe.waitForProcessToFinish (5000);
    setState (StreamState::idle);
    lastError.clear();
    return true;
}

bool StreamEngine::startStream()
{
    const auto currentState = state.load();

    if (currentState == StreamState::live || currentState == StreamState::starting)
        return false;

    const auto ffmpeg = findFfmpegExecutable();

    if (! ffmpeg.existsAsFile())
    {
        setState (StreamState::error,
                  juce::String::fromUTF8 (u8"FFmpeg \u304c\u898b\u3064\u304b\u308a\u307e\u305b\u3093\u3002"
                                          u8"ffmpeg.exe \u3092 PATH \u307e\u305f\u306f Drizzle.exe \u3068\u540c\u3058\u30d5\u30a9\u30eb\u30c0\u306b\u914d\u7f6e\u3057\u3066\u304f\u3060\u3055\u3044\u3002"));
        return false;
    }

    const auto cfg = getConfig();

    if (cfg.streamKey.isEmpty())
    {
        setState (StreamState::error,
                  juce::String::fromUTF8 (u8"\u30b9\u30c8\u30ea\u30fc\u30e0\u30ad\u30fc\u304c\u672a\u8a2d\u5b9a\u3067\u3059\u3002"
                                          u8"YouTube Studio \u306e\u30ad\u30fc\u6587\u5b57\u5217\u306e\u307f\u3092\u5165\u529b\u3057\u3066\u304f\u3060\u3055\u3044\u3002"
                                          u8"rtmp://... \u306f\u4e0d\u8981\u3067\u3059\u3002"));
        return false;
    }

    const auto rtmpUrl = getYoutubeRtmpUrl (cfg.streamKey);

    if (rtmpUrl.isEmpty())
    {
        setState (StreamState::error,
                  juce::String::fromUTF8 (u8"\u30b9\u30c8\u30ea\u30fc\u30e0\u30ad\u30fc\u304c\u4e0d\u6b63\u3067\u3059\u3002"
                                          u8"rtmp://a.rtmp.youtube.com/live2 \u3067\u306f\u306a\u304f\u3001"
                                          u8"\u305d\u306e\u5f8c\u306e\u30ad\u30fc\u6587\u5b57\u5217\u306e\u307f\u3092\u5165\u529b\u3057\u3066\u304f\u3060\u3055\u3044\u3002"));
        return false;
    }

    stopStream();

    if (! prepareStreamAssets (cfg.title))
    {
        setState (StreamState::error,
                  juce::String::fromUTF8 (u8"\u914d\u4fe1\u7528\u30d5\u30a1\u30a4\u30eb\u306e\u6e96\u5099\u306b\u5931\u6557\u3057\u307e\u3057\u305f\u3002"));
        return false;
    }

    audioFifo.reset();
    underrunCount.store (0);
    stopRequested.store (false);
    setState (StreamState::starting);

    const auto hasFont = streamFontFile.existsAsFile();
    const auto command = buildFfmpegCommand (ffmpeg, cfg.streamKey, cfg.title, hasFont);
    ffmpegProcess = std::make_unique<FfmpegProcess>();

    if (! ffmpegProcess->start (command, streamWorkDir.getFullPathName()))
    {
        ffmpegProcess.reset();
        clearActiveFfmpegPid();
        setState (StreamState::error,
                  juce::String::fromUTF8 (u8"FFmpeg \u306e\u8d77\u52d5\u306b\u5931\u6557\u3057\u307e\u3057\u305f\u3002"));
        return false;
    }

    writeActiveFfmpegPid (ffmpegProcess->getProcessId());

    streamThread = std::make_unique<StreamWorkerThread> (*this);
    streamThread->startThread();
    setState (StreamState::live);
    return true;
}

void StreamEngine::stopStream()
{
    stopRequested.store (true);

    if (streamThread != nullptr)
    {
        streamThread->stopThread (5000);
        streamThread.reset();
    }

    if (ffmpegProcess != nullptr)
    {
        ffmpegProcess->kill();
        ffmpegProcess.reset();
    }

    clearActiveFfmpegPid();

    if (streamWorkDir.isDirectory())
        streamWorkDir.deleteRecursively();

    streamFontFile = {};

    if (state.load() != StreamState::error)
        setState (StreamState::idle);
}

void StreamEngine::runStreamThread()
{
    if (ffmpegProcess == nullptr)
        return;

    std::vector<int16_t> chunk ((size_t) kFfmpegChunkFrames * 2, 0);
    std::vector<int16_t> silence ((size_t) kFfmpegChunkFrames * 2, 0);

    const double sr = juce::jmax (8000.0, sampleRate.load());
    const double chunkDurationMs = ((double) kFfmpegChunkFrames * 1000.0) / sr;
    const auto streamStartMs = (int64_t) juce::Time::getMillisecondCounterHiRes();
    int64_t samplesDelivered = 0;

    for (int i = 0; i < 30 && ! stopRequested.load(); ++i)
    {
        if (audioFifo.getNumReadyFrames() >= kFfmpegChunkFrames * 2)
            break;

        juce::Thread::sleep (100);
    }

    while (! stopRequested.load())
    {
        const auto targetMs = streamStartMs
                            + (int64_t) std::llround ((double) samplesDelivered * 1000.0 / sr);
        const auto nowMs = (int64_t) juce::Time::getMillisecondCounterHiRes();
        const int waitMs = (int) (targetMs - nowMs);

        if (waitMs > 0)
            juce::Thread::sleep (waitMs);
        else if (nowMs - targetMs > (int64_t) (chunkDurationMs * 8.0))
        {
            samplesDelivered = (int64_t) std::llround (((double) (nowMs - streamStartMs) * sr) / 1000.0);
            samplesDelivered = (samplesDelivered / kFfmpegChunkFrames) * kFfmpegChunkFrames;
        }

        const int frames = audioFifo.popInterleavedS16 (chunk.data(), kFfmpegChunkFrames);

        if (frames <= 0)
        {
            underrunCount.fetch_add (1);
            std::fill (chunk.begin(), chunk.end(), 0);
        }
        else if (frames < kFfmpegChunkFrames)
        {
            const auto samplesRead = (size_t) frames * 2;
            std::fill (chunk.begin() + (ptrdiff_t) samplesRead, chunk.end(), 0);
        }

        if (! ffmpegProcess->write (chunk.data(), (int) (chunk.size() * sizeof (int16_t))))
            break;

        if (! ffmpegProcess->isRunning())
            break;

        samplesDelivered += kFfmpegChunkFrames;
    }

    juce::String stderrText;

    if (ffmpegProcess != nullptr)
    {
        if (ffmpegProcess->isRunning())
            ffmpegProcess->kill();

        stderrText = ffmpegProcess->readStderr();
        ffmpegProcess.reset();
    }

    clearActiveFfmpegPid();

    const auto wasRemoteDisconnect = stderrText.contains ("-10054")
                                  || stderrText.containsIgnoreCase ("Error number -10054");

    if (! stopRequested.load() && wasRemoteDisconnect)
    {
        setState (StreamState::idle);
        return;
    }

    if (! stopRequested.load())
    {
        setState (StreamState::error,
                  juce::String::fromUTF8 (u8"\u914d\u4fe1\u304c\u4e2d\u65ad\u3055\u308c\u307e\u3057\u305f\u3002")
                      + (stderrText.isNotEmpty() ? "\n" + stderrText : juce::String()));
    }
}

juce::String StreamEngine::getStatusText() const
{
    return stateToStatusText (state.load(), lastError);
}

juce::String StreamEngine::getLastError() const
{
    return lastError;
}

double StreamEngine::getLiveDurationSeconds() const noexcept
{
    const auto start = liveStartMs.load();

    if (start <= 0 || state.load() != StreamState::live)
        return 0.0;

    return (juce::Time::getMillisecondCounterHiRes() - (double) start) * 0.001;
}

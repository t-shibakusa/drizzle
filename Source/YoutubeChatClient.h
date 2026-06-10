#pragma once



#include <juce_core/juce_core.h>

#include <juce_events/juce_events.h>

#include <atomic>



struct YoutubeChatMessage

{

    juce::String id;

    juce::String author;

    juce::String text;

    bool isOwner = false;

};



struct YoutubeApiConfig

{

    juce::String clientId;

    juce::String clientSecret;

    int redirectPort = 8765;

};



class YoutubeChatClient final : public juce::ChangeBroadcaster

{

public:

    YoutubeChatClient();

    ~YoutubeChatClient() override;



    YoutubeApiConfig getApiConfig() const;

    void setApiConfig (const YoutubeApiConfig& config);



    bool isAuthenticated() const;

    bool hasApiCredentials() const;

    void disconnectAccount();



    void beginOAuthFlow (std::function<void (bool success, const juce::String& message)> onFinished);

    void startPolling();

    void stopPolling();

    bool isPolling() const noexcept { return pollingActive.load(); }



    void sendChatMessage (const juce::String& text);



    juce::Array<YoutubeChatMessage> getMessages() const;

    int getConcurrentViewers() const noexcept { return concurrentViewers.load(); }

    juce::String getStatusText() const;

    juce::String getLiveStudioUrl() const;

    juce::String getLiveWatchUrl() const;

    void endActiveLiveBroadcastAsync (std::function<void (bool success, const juce::String& message)> onFinished = {});

    void beginActiveLiveBroadcastAsync (std::function<void (bool success, const juce::String& message)> onFinished = {});

    bool endActiveLiveBroadcast (juce::String& errorOut);

    bool beginActiveLiveBroadcast (juce::String& errorOut);

    void setActiveStreamKey (const juce::String& streamKey);

    void warmUpLiveSessionCacheAsync();

    void cancelPendingBroadcastOperations();

    void resetStreamSessionState();

    void setChatConnectWaitingStatus();

    void runPollLoop();

private:

    struct OAuthTokens

    {

        juce::String accessToken;

        juce::String refreshToken;

        int64_t expiresAtMs = 0;

    };



    void loadSettings();

    void saveSettings() const;



    bool ensureAccessToken (juce::String& error);

    bool refreshAccessToken (juce::String& error);

    bool fetchActiveLiveChatId (juce::String& error, bool requireLiveChatId = true);

    bool resolveLiveChatSession (juce::String& error);

    bool resolveBroadcastIdForEnd (juce::String& broadcastId, juce::String& errorOut, bool forEnding = true);

    bool findBroadcastIdViaActiveStream (juce::String& broadcastId, juce::String& liveChatId, juce::String& errorOut);

    bool findBroadcastIdViaStreamKey (juce::String& broadcastId, juce::String& liveChatId, juce::String& errorOut);

    bool findBroadcastIdViaLiveVideoSearch (juce::String& broadcastId, juce::String& errorOut);

    bool findLiveVideoIdForOwnChannel (juce::String& videoId, juce::String& errorOut) const;

    bool fetchLiveChatIdFromVideoId (const juce::String& videoId, juce::String& liveChatIdOut, juce::String& errorOut) const;

    bool fetchLiveChatIdFromBroadcastId (const juce::String& broadcastId, juce::String& liveChatIdOut, juce::String& errorOut) const;

    void noteYoutubeApiError (juce::String& errorOut, const juce::var& apiError) const;

    bool findBroadcastIdViaAnyLiveBroadcast (juce::String& broadcastId, juce::String& liveChatId, juce::String& errorOut);

    bool selectBestBroadcastFromItems (const juce::var& items, juce::String& broadcastId, juce::String& liveChatId) const;

    bool selectBestLiveBroadcastFromItems (const juce::var& items, juce::String& broadcastId, juce::String& liveChatId) const;

    bool findLiveChatIdFromBroadcastList (juce::String& broadcastId, juce::String& liveChatId, juce::String& errorOut);

    bool findLiveChatFromActiveBroadcasts (juce::String& broadcastId, juce::String& liveChatId, juce::String& errorOut);

    bool findActiveLiveChatFromLiveVideos (juce::String& videoId, juce::String& liveChatId, juce::String& errorOut) const;

    void setChatConnectRetryingStatus();

    bool isBroadcastAlreadyComplete (const juce::String& broadcastId, juce::String& errorOut) const;

    bool getBroadcastLifeCycle (const juce::String& broadcastId, juce::String& lifeCycleOut, juce::String& errorOut) const;

    bool transitionBroadcastStatus (const juce::String& broadcastId,
                                    const juce::String& targetStatus,
                                    juce::String& errorOut);

    bool pollChatOnce (juce::String& error);

    bool postChatMessage (const juce::String& text, juce::String& error);

    void updateConcurrentViewers();



    juce::String buildAuthUrl (const juce::String& state) const;

    bool exchangeAuthCode (const juce::String& code, juce::String& error);

    juce::String httpGet (const juce::String& url, juce::String& error) const;

    juce::String httpPostForm (const juce::String& url,

                               const juce::String& formBody,

                               juce::String& error) const;

    juce::String httpPostJson (const juce::String& url,

                               const juce::String& jsonBody,

                               const juce::String& bearerToken,

                               juce::String& error) const;

    juce::String httpPostBearer (const juce::String& url, juce::String& error) const;



    void setStatus (const juce::String& text);

    void appendMessage (const YoutubeChatMessage& message);



    static juce::File getSettingsFile();



    mutable juce::CriticalSection lock;

    YoutubeApiConfig apiConfig;

    OAuthTokens tokens;

    juce::Array<YoutubeChatMessage> messages;

    juce::StringArray seenMessageIds;

    juce::String liveChatId;

    juce::String activeBroadcastId;

    juce::String activeLiveVideoId;

    juce::String activeStreamKey;

    juce::String nextPageToken;

    juce::String statusText { juce::String::fromUTF8 (u8"\u672a\u9023\u643a") };

    int pollingIntervalMs = 5000;



    std::unique_ptr<juce::Thread> pollThread;

    std::unique_ptr<juce::Thread> oauthThread;

    std::unique_ptr<juce::Thread> endBroadcastThread;

    std::unique_ptr<juce::Thread> startBroadcastThread;

    std::atomic<bool> pollingActive { false };

    std::atomic<bool> stopPollRequested { false };

    std::atomic<int> chatConnectFailCount { 0 };

    static constexpr int kChatConnectGraceAttempts = 6;

    std::atomic<int64_t> chatPollStartMs { 0 };

    std::atomic<int64_t> lastChatResolveAttemptMs { 0 };

    mutable std::atomic<bool> apiQuotaExceeded { false };

    static constexpr int kChatResolveRetryIntervalMs = 15000;

    std::atomic<int> concurrentViewers { 0 };



    static constexpr int kMaxStoredMessages = 300;

};



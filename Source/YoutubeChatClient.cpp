#include "YoutubeChatClient.h"
#include "StreamEngine.h"

#include <juce_core/juce_core.h>
#include <thread>



namespace

{

constexpr auto kAuthScope = "https://www.googleapis.com/auth/youtube https://www.googleapis.com/auth/youtube.readonly https://www.googleapis.com/auth/youtube.force-ssl";



juce::String urlEncode (juce::String text)

{

    return juce::URL::addEscapeChars (text, true);

}



juce::String generateStateToken()

{

    return juce::Uuid().toString();

}



juce::String getBroadcastLifeCycleStatus (const juce::var& item)
{
    if (! item.isObject())
        return {};

    if (auto* broadcast = item.getDynamicObject())
    {
        if (auto* status = broadcast->getProperty ("status").getDynamicObject())
            return status->getProperty ("lifeCycleStatus").toString();
    }

    return {};
}

bool isActiveLiveBroadcast (const juce::var& item)
{
    const auto lifeCycle = getBroadcastLifeCycleStatus (item);
    return lifeCycle == "live" || lifeCycle == "testing" || lifeCycle == "liveStarting";
}

int endableBroadcastPriority (const juce::String& lifeCycle)
{
    if (lifeCycle == "live")
        return 0;

    if (lifeCycle == "liveStarting")
        return 1;

    if (lifeCycle == "testing")
        return 2;

    if (lifeCycle == "ready")
        return 3;

    if (lifeCycle == "created")
        return 4;

    return 100;
}

bool isEndableBroadcast (const juce::var& item)
{
    const auto lifeCycle = getBroadcastLifeCycleStatus (item);
    return lifeCycle.isNotEmpty() && lifeCycle != "complete" && lifeCycle != "revoked";
}

bool extractLiveBroadcastFields (const juce::var& item,
                                 juce::String& broadcastId,
                                 juce::String& liveChatId)
{
    if (! item.isObject())
        return false;

    auto* broadcast = item.getDynamicObject();

    if (broadcast == nullptr)
        return false;

    broadcastId = broadcast->getProperty ("id").toString();
    liveChatId.clear();

    if (auto* snippet = broadcast->getProperty ("snippet").getDynamicObject())
        liveChatId = snippet->getProperty ("liveChatId").toString();

    return broadcastId.isNotEmpty();
}

juce::String extractYoutubeApiErrorMessage (const juce::var& apiError)
{
    if (auto* errObj = apiError.getDynamicObject())
    {
        const auto topMessage = errObj->getProperty ("message").toString();

        if (topMessage.isNotEmpty() && ! topMessage.startsWithIgnoreCase ("Object "))
            return topMessage;

        if (auto* errors = errObj->getProperty ("errors").getArray())
        {
            for (const auto& item : *errors)
            {
                if (auto* detail = item.getDynamicObject())
                {
                    const auto detailMessage = detail->getProperty ("message").toString();
                    const auto reason = detail->getProperty ("reason").toString();

                    if (detailMessage.isNotEmpty())
                        return detailMessage;

                    if (reason.isNotEmpty())
                        return reason;
                }
            }
        }

        const int code = (int) errObj->getProperty ("code");

        if (code != 0)
            return "YouTube API error (HTTP " + juce::String (code) + ")";
    }

    const auto fallback = apiError.toString();

    if (fallback.startsWithIgnoreCase ("Object "))
        return juce::String::fromUTF8 (u8"YouTube API \u30a8\u30e9\u30fc\u304c\u767a\u751f\u3057\u307e\u3057\u305f\u3002");

    return fallback;
}

bool isYoutubeQuotaExceededError (const juce::String& message)
{
    return message.containsIgnoreCase ("quota exceeded");
}

juce::String formatYoutubeQuotaExceededMessage()
{
    return juce::String::fromUTF8 (u8"YouTube API \u306e1\u65e5\u306e\u5229\u7528\u4e0a\u9650\u306b\u9054\u3057\u307e\u3057\u305f\u3002"
                                  u8"\u4eca\u65e5\u306f\u30c1\u30e3\u30c3\u30c8\u63a5\u7d9a\u3092\u7d9a\u3051\u3089\u308c\u307e\u305b\u3093\u3002"
                                  u8"\uff08\u65e5\u672c\u6642\u9593\u306e\u5348\u5f8c\u306b\u30ea\u30bb\u30c3\u30c8\u3055\u308c\u307e\u3059\uff09"
                                  u8"\nGoogle Cloud Console \u3067 YouTube Data API v3 \u306e\u30af\u30a9\u30fc\u30bf\u7533\u8acb\u3092\u3059\u308b\u3068\u89e3\u6c7a\u3067\u304d\u307e\u3059\u3002");
}

juce::String extractQueryParam (const juce::String& query, const juce::String& key)

{

    for (auto& part : juce::StringArray::fromTokens (query, "&", ""))

    {

        const auto keyPart = part.upToFirstOccurrenceOf ("=", false, false);

        if (keyPart == key)

            return juce::URL::removeEscapeChars (part.fromFirstOccurrenceOf ("=", false, false));

    }



    return {};

}



juce::String parseHttpRequestTarget (const juce::String& request)

{

    const auto firstLine = request.upToFirstOccurrenceOf ("\n", false, false).trim();

    const auto parts = juce::StringArray::fromTokens (firstLine, " ", "");

    return parts.size() >= 2 ? parts[1] : juce::String();

}



juce::String readHttpRequest (juce::StreamingSocket& socket, int timeoutMs)

{

    juce::String request;

    char chunk[1024];

    const auto deadlineMs = juce::Time::getMillisecondCounterHiRes() + (double) timeoutMs;



    while (request.length() < 16384 && juce::Time::getMillisecondCounterHiRes() < deadlineMs)

    {

        const auto remainingMs = (int) (deadlineMs - juce::Time::getMillisecondCounterHiRes());

        const int ready = socket.waitUntilReady (true, juce::jmax (1, juce::jmin (500, remainingMs)));



        if (ready > 0)

        {

            const int bytesRead = socket.read (chunk, (int) sizeof (chunk), false);



            if (bytesRead > 0)

            {

                request += juce::String::fromUTF8 (chunk, (size_t) bytesRead);



                if (request.contains ("\r\n\r\n"))

                    return request;

            }

        }

        else if (ready < 0)

        {

            break;

        }

    }



    return request;

}



juce::String makeOAuthSuccessHtml()

{

    return juce::String::fromUTF8 (

        u8"<html><body><h2>Drizzle</h2><p>YouTube \u9023\u643a\u304c\u5b8c\u4e86\u3057\u307e\u3057\u305f\u3002\u3053\u306e\u30bf\u30d6\u3092\u9589\u3058\u3066\u30a2\u30d7\u30ea\u306b\u623b\u3063\u3066\u304f\u3060\u3055\u3044\u3002</p></body></html>");

}



void writeHttpResponse (juce::StreamingSocket& socket, const juce::String& body)

{

    const auto response = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\nContent-Length: "

                        + juce::String (body.getNumBytesAsUTF8())

                        + "\r\n\r\n" + body;

    socket.write (response.toRawUTF8(), response.getNumBytesAsUTF8());

    socket.close();

}



class OAuthLoopbackThread final : public juce::Thread

{

public:

    OAuthLoopbackThread (int portIn, juce::String expectedStateIn)

        : juce::Thread ("YouTube OAuth"),

          port (portIn),

          expectedState (std::move (expectedStateIn))

    {

    }



    juce::String getAuthCode() const

    {

        const juce::ScopedLock sl (resultLock);

        return authCode;

    }



    juce::String getError() const

    {

        const juce::ScopedLock sl (resultLock);

        return error;

    }



    void run() override

    {

        juce::StreamingSocket server;



        if (! server.createListener (port, "127.0.0.1"))

        {

            setResult ({}, juce::String::fromUTF8 (u8"OAuth \u30dd\u30fc\u30c8 ") + juce::String (port)

                              + juce::String::fromUTF8 (u8" \u3067\u30ea\u30b9\u30ca\u30fc\u3092\u958b\u3051\u3089\u308c\u307e\u305b\u3093\u3067\u3057\u305f\u3002"));

            return;

        }



        const auto deadlineMs = juce::Time::getMillisecondCounterHiRes() + 120000.0;



        while (! threadShouldExit() && juce::Time::getMillisecondCounterHiRes() < deadlineMs)

        {

            if (server.waitUntilReady (true, 1000) <= 0)

                continue;



            std::unique_ptr<juce::StreamingSocket> client (server.waitForNextConnection());



            if (client == nullptr)

                continue;



            const auto request = readHttpRequest (*client, 15000);



            if (request.isNotEmpty())

                writeHttpResponse (*client, makeOAuthSuccessHtml());



            if (request.isEmpty())

                continue;



            const auto target = parseHttpRequestTarget (request);

            const auto query = target.fromFirstOccurrenceOf ("?", false, false);

            const auto code = extractQueryParam (query, "code");

            const auto state = extractQueryParam (query, "state");

            const auto oauthError = extractQueryParam (query, "error");



            if (code.isEmpty() && oauthError.isEmpty())

                continue;



            if (oauthError.isNotEmpty())

            {

                auto message = oauthError;

                const auto description = extractQueryParam (query, "error_description");

                if (description.isNotEmpty())

                    message += "\n" + description;



                if (oauthError == "access_denied")

                    message += juce::String::fromUTF8 (u8"\n\n\u30d2\u30f3\u30c8: OAuth \u540c\u610f\u753b\u9762\u3067\u300c\u30c6\u30b9\u30c8\u30e6\u30fc\u30b6\u30fc\u300d\u306b\u81ea\u5206\u306e Gmail \u304c\u8ffd\u52a0\u3055\u308c\u3066\u3044\u308b\u304b\u78ba\u8a8d\u3057\u3066\u304f\u3060\u3055\u3044\u3002");



                setResult ({}, message);

                return;

            }



            if (state != expectedState)

            {

                setResult ({}, juce::String::fromUTF8 (u8"OAuth \u72b6\u614b\u30c8\u30fc\u30af\u30f3\u304c\u4e00\u81f4\u3057\u307e\u305b\u3093\u3002"));

                return;

            }



            if (code.isEmpty())

            {

                setResult ({}, juce::String::fromUTF8 (u8"\u8a8d\u8a3c\u30b3\u30fc\u30c9\u3092\u53d6\u5f97\u3067\u304d\u307e\u305b\u3093\u3067\u3057\u305f\u3002"));

                return;

            }



            setResult (code, {});

            return;

        }



        setResult ({}, juce::String::fromUTF8 (u8"Google \u30ed\u30b0\u30a4\u30f3\u306e\u5fdc\u7b54\u304c\u30bf\u30a4\u30e0\u30a2\u30a6\u30c8\u3057\u307e\u3057\u305f\u3002"));

    }



private:

    void setResult (const juce::String& code, const juce::String& errorIn)

    {

        const juce::ScopedLock sl (resultLock);

        authCode = code;

        error = errorIn;

    }



    int port = 8765;

    juce::String expectedState;

    juce::CriticalSection resultLock;

    juce::String authCode;

    juce::String error;

};



class ChatPollThread final : public juce::Thread

{

public:

    explicit ChatPollThread (YoutubeChatClient& clientIn)

        : juce::Thread ("YouTube Chat Poll"),

          client (clientIn)

    {

    }



    void run() override

    {

        client.runPollLoop();

    }



private:

    YoutubeChatClient& client;

};



class EndBroadcastThread final : public juce::Thread

{

public:

    EndBroadcastThread (YoutubeChatClient& clientIn,

                        std::function<void (bool, const juce::String&)> onFinishedIn)

        : juce::Thread ("YouTube End Broadcast"),

          client (clientIn),

          onFinished (std::move (onFinishedIn))

    {

    }



    void run() override

    {

        juce::String errorOut;

        const bool success = client.endActiveLiveBroadcast (errorOut);

        const auto message = success

                                 ? juce::String::fromUTF8 (u8"YouTube \u914d\u4fe1\u3092\u7d42\u4e86\u3057\u307e\u3057\u305f\u3002")

                                 : errorOut;



        if (onFinished != nullptr)

        {

            juce::MessageManager::callAsync ([callback = onFinished, success, message]

                                             {

                                                 callback (success, message);

                                             });

        }

    }



private:

    YoutubeChatClient& client;

    std::function<void (bool, const juce::String&)> onFinished;

};



class StartBroadcastThread final : public juce::Thread

{

public:

    StartBroadcastThread (YoutubeChatClient& clientIn,

                          std::function<void (bool, const juce::String&)> onFinishedIn)

        : juce::Thread ("YouTube Start Broadcast"),

          client (clientIn),

          onFinished (std::move (onFinishedIn))

    {

    }



    void run() override

    {

        juce::String errorOut;

        const bool success = client.beginActiveLiveBroadcast (errorOut);

        const auto message = success

                                 ? juce::String::fromUTF8 (u8"YouTube \u914d\u4fe1\u3092\u958b\u59cb\u3057\u307e\u3057\u305f\u3002")

                                 : errorOut;



        if (onFinished != nullptr)

        {

            juce::MessageManager::callAsync ([callback = onFinished, success, message]

                                             {

                                                 callback (success, message);

                                             });

        }

    }



private:

    YoutubeChatClient& client;

    std::function<void (bool, const juce::String&)> onFinished;

};

} // namespace



juce::File YoutubeChatClient::getSettingsFile()

{

    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)

               .getChildFile ("Drizzle")

               .getChildFile ("youtube_settings.xml");

}



YoutubeChatClient::YoutubeChatClient()

{

    loadSettings();

}



YoutubeChatClient::~YoutubeChatClient()

{

    stopPolling();



    if (endBroadcastThread != nullptr)

    {

        endBroadcastThread->stopThread (5000);

        endBroadcastThread.reset();

    }



    if (startBroadcastThread != nullptr)

    {

        startBroadcastThread->stopThread (5000);

        startBroadcastThread.reset();

    }



    oauthThread.reset();

}



void YoutubeChatClient::loadSettings()

{

    const juce::ScopedLock sl (lock);

    apiConfig = {};

    tokens = {};



    const auto file = getSettingsFile();



    if (! file.existsAsFile())

        return;



    if (auto xml = juce::XmlDocument::parse (file))

    {

        if (auto* api = xml->getChildByName ("API"))

        {

            apiConfig.clientId = api->getStringAttribute ("clientId");

            apiConfig.clientSecret = api->getStringAttribute ("clientSecret");

            apiConfig.redirectPort = api->getIntAttribute ("redirectPort", apiConfig.redirectPort);

        }



        if (auto* tokenNode = xml->getChildByName ("TOKENS"))

        {

            tokens.accessToken = tokenNode->getStringAttribute ("accessToken");

            tokens.refreshToken = tokenNode->getStringAttribute ("refreshToken");

            tokens.expiresAtMs = tokenNode->getStringAttribute ("expiresAtMs").getLargeIntValue();

        }

    }



    if (isAuthenticated())

        statusText = juce::String::fromUTF8 (u8"YouTube \u9023\u643a\u6e08\u307f");

}



void YoutubeChatClient::saveSettings() const

{

    const juce::ScopedLock sl (lock);



    juce::XmlElement root ("YOUTUBE");

    auto* api = root.createNewChildElement ("API");

    api->setAttribute ("clientId", apiConfig.clientId);

    api->setAttribute ("clientSecret", apiConfig.clientSecret);

    api->setAttribute ("redirectPort", apiConfig.redirectPort);



    auto* tokenNode = root.createNewChildElement ("TOKENS");

    tokenNode->setAttribute ("accessToken", tokens.accessToken);

    tokenNode->setAttribute ("refreshToken", tokens.refreshToken);

    tokenNode->setAttribute ("expiresAtMs", juce::String (tokens.expiresAtMs));



    const auto file = getSettingsFile();

    file.getParentDirectory().createDirectory();

    root.writeTo (file, {});

}



YoutubeApiConfig YoutubeChatClient::getApiConfig() const

{

    const juce::ScopedLock sl (lock);

    return apiConfig;

}



void YoutubeChatClient::setApiConfig (const YoutubeApiConfig& config)

{

    {

        const juce::ScopedLock sl (lock);

        apiConfig = config;

    }

    saveSettings();

}



bool YoutubeChatClient::hasApiCredentials() const

{

    const juce::ScopedLock sl (lock);

    return apiConfig.clientId.trim().isNotEmpty() && apiConfig.clientSecret.trim().isNotEmpty();

}



bool YoutubeChatClient::isAuthenticated() const

{

    const juce::ScopedLock sl (lock);

    return tokens.refreshToken.isNotEmpty() || tokens.accessToken.isNotEmpty();

}



void YoutubeChatClient::disconnectAccount()

{

    stopPolling();



    {

        const juce::ScopedLock sl (lock);

        tokens = {};

        liveChatId.clear();

        activeBroadcastId.clear();

        activeLiveVideoId.clear();

        activeStreamKey.clear();

        nextPageToken.clear();

        messages.clear();

        seenMessageIds.clear();

        concurrentViewers.store (0);

        statusText = juce::String::fromUTF8 (u8"\u672a\u9023\u643a");

    }



    saveSettings();

    sendChangeMessage();

}



juce::String YoutubeChatClient::getStatusText() const

{

    const juce::ScopedLock sl (lock);

    return statusText;

}



juce::String YoutubeChatClient::getLiveStudioUrl() const

{

    const juce::ScopedLock sl (lock);

    if (activeBroadcastId.isEmpty())

        return {};



    return "https://studio.youtube.com/video/" + activeBroadcastId + "/livestreaming";

}



juce::String YoutubeChatClient::getLiveWatchUrl() const

{

    const juce::ScopedLock sl (lock);

    if (activeBroadcastId.isEmpty())

        return {};



    return "https://www.youtube.com/watch?v=" + activeBroadcastId;

}



juce::Array<YoutubeChatMessage> YoutubeChatClient::getMessages() const

{

    const juce::ScopedLock sl (lock);

    return messages;

}



void YoutubeChatClient::setStatus (const juce::String& text)

{

    {

        const juce::ScopedLock sl (lock);

        statusText = text;

    }

    sendChangeMessage();

}



void YoutubeChatClient::appendMessage (const YoutubeChatMessage& message)

{

    bool added = false;



    {

        const juce::ScopedLock sl (lock);



        if (message.id.isNotEmpty() && seenMessageIds.contains (message.id))

            return;



        if (message.id.isNotEmpty())

            seenMessageIds.add (message.id);



        messages.add (message);



        while (messages.size() > kMaxStoredMessages)

        {

            const auto removed = messages.removeAndReturn (0);



            if (removed.id.isNotEmpty())

                seenMessageIds.removeString (removed.id);

        }



        added = true;

    }



    if (added)

        sendChangeMessage();

}



juce::String YoutubeChatClient::httpGet (const juce::String& url, juce::String& error) const

{

    juce::String bearer;

    {

        const juce::ScopedLock sl (lock);

        bearer = tokens.accessToken;

    }



    const auto response = juce::URL (url)
                              .createInputStream (juce::URL::InputStreamOptions ({})
                                                      .withExtraHeaders ("Authorization: Bearer " + bearer)
                                                      .withConnectionTimeoutMs (15000));



    if (response == nullptr)

    {

        error = juce::String::fromUTF8 (u8"YouTube API \u3078\u306e\u63a5\u7d9a\u306b\u5931\u6557\u3057\u307e\u3057\u305f\u3002");

        return {};

    }



    return response->readEntireStreamAsString();

}



juce::String YoutubeChatClient::httpPostForm (const juce::String& url,

                                              const juce::String& formBody,

                                              juce::String& error) const

{

    const auto response = juce::URL (url)
                              .withPOSTData (formBody)
                              .createInputStream (juce::URL::InputStreamOptions ({})
                                                      .withExtraHeaders ("Content-Type: application/x-www-form-urlencoded")
                                                      .withHttpRequestCmd ("POST")
                                                      .withConnectionTimeoutMs (15000));



    if (response == nullptr)

    {

        error = juce::String::fromUTF8 (u8"OAuth \u30c8\u30fc\u30af\u53d6\u5f97\u306b\u5931\u6557\u3057\u307e\u3057\u305f\u3002");

        return {};

    }



    return response->readEntireStreamAsString();

}



juce::String YoutubeChatClient::httpPostJson (const juce::String& url,

                                              const juce::String& jsonBody,

                                              const juce::String& bearerToken,

                                              juce::String& error) const

{

    const auto response = juce::URL (url)
                              .withPOSTData (jsonBody)
                              .createInputStream (juce::URL::InputStreamOptions ({})
                                                      .withExtraHeaders ("Content-Type: application/json\r\nAuthorization: Bearer " + bearerToken)
                                                      .withHttpRequestCmd ("POST")
                                                      .withConnectionTimeoutMs (15000));



    if (response == nullptr)

    {

        error = juce::String::fromUTF8 (u8"\u30b3\u30e1\u30f3\u30c8\u9001\u4fe1\u306b\u5931\u6557\u3057\u307e\u3057\u305f\u3002");

        return {};

    }



    return response->readEntireStreamAsString();

}



juce::String YoutubeChatClient::httpPostBearer (const juce::String& url, juce::String& error) const

{

    juce::String bearer;



    {

        const juce::ScopedLock sl (lock);

        bearer = tokens.accessToken;

    }



    const auto response = juce::URL (url)
                              .withPOSTData (juce::String())
                              .createInputStream (juce::URL::InputStreamOptions ({})
                                                      .withExtraHeaders ("Authorization: Bearer " + bearer)
                                                      .withHttpRequestCmd ("POST")
                                                      .withConnectionTimeoutMs (15000));



    if (response == nullptr)

    {

        error = juce::String::fromUTF8 (u8"YouTube API \u3078\u306e\u63a5\u7d9a\u306b\u5931\u6557\u3057\u307e\u3057\u305f\u3002");

        return {};

    }



    return response->readEntireStreamAsString();

}



juce::String YoutubeChatClient::buildAuthUrl (const juce::String& state) const

{

    const juce::ScopedLock sl (lock);

    const auto redirectUri = "http://127.0.0.1:" + juce::String (apiConfig.redirectPort) + "/";



    return juce::String ("https://accounts.google.com/o/oauth2/v2/auth?response_type=code")
         + "&client_id=" + urlEncode (apiConfig.clientId)

         + "&redirect_uri=" + urlEncode (redirectUri)

         + "&scope=" + urlEncode (kAuthScope)

         + "&access_type=offline"

         + "&prompt=consent"

         + "&state=" + urlEncode (state);

}



bool YoutubeChatClient::exchangeAuthCode (const juce::String& code, juce::String& error)

{

    YoutubeApiConfig configCopy;

    {

        const juce::ScopedLock sl (lock);

        configCopy = apiConfig;

    }



    const auto redirectUri = "http://127.0.0.1:" + juce::String (configCopy.redirectPort) + "/";

    const auto body = "code=" + urlEncode (code)

                    + "&client_id=" + urlEncode (configCopy.clientId)

                    + "&client_secret=" + urlEncode (configCopy.clientSecret)

                    + "&redirect_uri=" + urlEncode (redirectUri)

                    + "&grant_type=authorization_code";



    const auto response = httpPostForm ("https://oauth2.googleapis.com/token", body, error);



    if (response.isEmpty())

        return false;



    const auto json = juce::JSON::parse (response);



    if (! json.isObject())

    {

        error = response;

        return false;

    }



    if (auto* obj = json.getDynamicObject())

    {

        if (obj->hasProperty ("error"))

        {

            error = obj->getProperty ("error_description").toString();



            if (error.isEmpty())

                error = extractYoutubeApiErrorMessage (obj->getProperty ("error"));



            return false;

        }



        const juce::ScopedLock sl (lock);

        tokens.accessToken = obj->getProperty ("access_token").toString();

        const auto refresh = obj->getProperty ("refresh_token").toString();



        if (refresh.isNotEmpty())

            tokens.refreshToken = refresh;



        const int expiresIn = (int) obj->getProperty ("expires_in");

        tokens.expiresAtMs = (int64_t) juce::Time::getMillisecondCounterHiRes()

                           + (int64_t) juce::jmax (30, expiresIn) * 1000;

    }



    saveSettings();

    return true;

}



bool YoutubeChatClient::refreshAccessToken (juce::String& error)

{

    juce::String refreshToken;

    YoutubeApiConfig configCopy;



    {

        const juce::ScopedLock sl (lock);

        refreshToken = tokens.refreshToken;

        configCopy = apiConfig;

    }



    if (refreshToken.isEmpty())

    {

        error = juce::String::fromUTF8 (u8"refresh token \u304c\u3042\u308a\u307e\u305b\u3093\u3002\u518d\u9023\u643a\u3057\u3066\u304f\u3060\u3055\u3044\u3002");

        return false;

    }



    const auto body = "client_id=" + urlEncode (configCopy.clientId)

                    + "&client_secret=" + urlEncode (configCopy.clientSecret)

                    + "&refresh_token=" + urlEncode (refreshToken)

                    + "&grant_type=refresh_token";



    const auto response = httpPostForm ("https://oauth2.googleapis.com/token", body, error);



    if (response.isEmpty())

        return false;



    const auto json = juce::JSON::parse (response);



    if (! json.isObject())

    {

        error = response;

        return false;

    }



    if (auto* obj = json.getDynamicObject())

    {

        if (obj->hasProperty ("error"))

        {

            error = obj->getProperty ("error_description").toString();



            if (error.isEmpty())

                error = extractYoutubeApiErrorMessage (obj->getProperty ("error"));



            return false;

        }



        const juce::ScopedLock sl (lock);

        tokens.accessToken = obj->getProperty ("access_token").toString();

        const int expiresIn = (int) obj->getProperty ("expires_in");

        tokens.expiresAtMs = (int64_t) juce::Time::getMillisecondCounterHiRes()

                           + (int64_t) juce::jmax (30, expiresIn) * 1000;

    }



    saveSettings();

    return true;

}



bool YoutubeChatClient::ensureAccessToken (juce::String& error)

{

    int64_t expiresAt = 0;

    juce::String accessToken;



    {

        const juce::ScopedLock sl (lock);

        expiresAt = tokens.expiresAtMs;

        accessToken = tokens.accessToken;

    }



    if (accessToken.isNotEmpty()

        && expiresAt > (int64_t) juce::Time::getMillisecondCounterHiRes() + 60000)

        return true;



    return refreshAccessToken (error);

}



bool YoutubeChatClient::findLiveChatIdFromBroadcastList (juce::String& broadcastId,

                                                         juce::String& liveChatId,

                                                         juce::String& errorOut)

{

    const juce::StringArray broadcastTypes { "all", "persistent", "event" };



    juce::String bestId;

    juce::String bestChatId;

    int bestPriority = 1000;



    for (const auto& broadcastType : broadcastTypes)

    {

        const auto response = httpGet ("https://www.googleapis.com/youtube/v3/liveBroadcasts?part=snippet,status"

                                       "&mine=true&broadcastType=" + broadcastType + "&maxResults=50",

                                       errorOut);



        if (response.isEmpty())

            continue;



        const auto json = juce::JSON::parse (response);



        if (! json.isObject())

            continue;



        auto* root = json.getDynamicObject();



        if (root == nullptr)

            continue;



        if (root->hasProperty ("error"))

        {

            noteYoutubeApiError (errorOut, root->getProperty ("error"));

            continue;

        }



        const auto items = root->getProperty ("items");



        if (! items.isArray())

            continue;



        for (const auto& item : *items.getArray())

        {

            if (! isEndableBroadcast (item))

                continue;



            juce::String candidateId;

            juce::String candidateChatId;



            if (! extractLiveBroadcastFields (item, candidateId, candidateChatId))

                continue;



            if (candidateChatId.isEmpty())

            {

                juce::String detailError;

                fetchLiveChatIdFromBroadcastId (candidateId, candidateChatId, detailError);

            }



            if (candidateChatId.isEmpty())

                continue;



            auto priority = endableBroadcastPriority (getBroadcastLifeCycleStatus (item));



            if (isActiveLiveBroadcast (item))

                priority -= 100;



            if (priority < bestPriority)

            {

                bestPriority = priority;

                bestId = candidateId;

                bestChatId = candidateChatId;

            }

        }

    }



    if (bestId.isEmpty() || bestChatId.isEmpty())

        return false;



    broadcastId = bestId;

    liveChatId = bestChatId;

    return true;

}



bool YoutubeChatClient::findLiveChatFromActiveBroadcasts (juce::String& broadcastId,

                                                          juce::String& liveChatId,

                                                          juce::String& errorOut)

{

    const juce::StringArray broadcastTypes { "all", "persistent", "event" };



    for (const auto& broadcastType : broadcastTypes)

    {

        const auto response = httpGet ("https://www.googleapis.com/youtube/v3/liveBroadcasts?part=snippet,status"

                                       "&mine=true&broadcastType=" + broadcastType + "&maxResults=50",

                                       errorOut);



        if (response.isEmpty())

            continue;



        const auto json = juce::JSON::parse (response);



        if (! json.isObject())

            continue;



        auto* root = json.getDynamicObject();



        if (root == nullptr || root->hasProperty ("error"))

            continue;



        const auto items = root->getProperty ("items");



        if (! items.isArray())

            continue;



        for (const auto& item : *items.getArray())

        {

            if (! isActiveLiveBroadcast (item))

                continue;



            juce::String candidateId;

            juce::String ignoredChatId;



            if (! extractLiveBroadcastFields (item, candidateId, ignoredChatId))

                continue;



            const auto detailResponse = httpGet ("https://www.googleapis.com/youtube/v3/liveBroadcasts?part=snippet&id="

                                                 + urlEncode (candidateId),

                                               errorOut);



            if (detailResponse.isEmpty())

                continue;



            const auto detailJson = juce::JSON::parse (detailResponse);



            if (! detailJson.isObject())

                continue;



            if (auto* detailRoot = detailJson.getDynamicObject())

            {

                const auto detailItems = detailRoot->getProperty ("items");



                if (detailItems.isArray() && ! detailItems.getArray()->isEmpty())

                {

                    juce::String detailId;

                    juce::String detailChatId;



                    if (extractLiveBroadcastFields (detailItems.getArray()->getReference (0), detailId, detailChatId)

                        && detailChatId.isNotEmpty())

                    {

                        broadcastId = detailId;

                        liveChatId = detailChatId;

                        return true;

                    }

                }

            }

        }

    }



    return false;

}



bool YoutubeChatClient::findActiveLiveChatFromLiveVideos (juce::String& videoId,

                                                          juce::String& liveChatId,

                                                          juce::String& errorOut) const

{

    if (apiQuotaExceeded.load())

        return false;



    const juce::StringArray broadcastTypes { "all", "event", "persistent" };



    for (const auto& broadcastType : broadcastTypes)

    {

        const auto response = httpGet ("https://www.googleapis.com/youtube/v3/liveBroadcasts?part=snippet,status"

                                       "&mine=true&broadcastStatus=active&broadcastType=" + broadcastType + "&maxResults=10",

                                       errorOut);



        if (response.isEmpty())

            continue;



        const auto json = juce::JSON::parse (response);



        if (! json.isObject())

            continue;



        if (auto* root = json.getDynamicObject())

        {

            if (root->hasProperty ("error"))

            {

                noteYoutubeApiError (errorOut, root->getProperty ("error"));

                continue;

            }



            const auto items = root->getProperty ("items");



            if (! items.isArray())

                continue;



            for (const auto& item : *items.getArray())

            {

                juce::String candidateId;

                juce::String candidateChatId;



                if (! extractLiveBroadcastFields (item, candidateId, candidateChatId))

                    continue;



                if (candidateChatId.isEmpty())

                {

                    juce::String detailError;

                    fetchLiveChatIdFromBroadcastId (candidateId, candidateChatId, detailError);

                }



                if (candidateChatId.isEmpty())

                    continue;



                videoId = candidateId;

                liveChatId = candidateChatId;

                return true;

            }

        }

    }



    return false;

}



bool YoutubeChatClient::resolveLiveChatSession (juce::String& error)

{

    {

        const juce::ScopedLock sl (lock);

        if (liveChatId.isNotEmpty())

            return true;

    }



    if (apiQuotaExceeded.load())

    {

        error = formatYoutubeQuotaExceededMessage();

        return false;

    }



    const auto nowMs = (int64_t) juce::Time::getMillisecondCounterHiRes();

    const auto lastAttemptMs = lastChatResolveAttemptMs.load();



    if (lastAttemptMs > 0 && (nowMs - lastAttemptMs) < kChatResolveRetryIntervalMs)

    {

        error.clear();

        return false;

    }



    lastChatResolveAttemptMs.store (nowMs);



    if (! ensureAccessToken (error))

        return false;



    juce::String videoId;

    juce::String broadcastId;

    juce::String chatId;

    juce::String localError;



    {

        juce::String broadcastChatId;



        if (findBroadcastIdViaStreamKey (broadcastId, broadcastChatId, localError)

            || findBroadcastIdViaActiveStream (broadcastId, broadcastChatId, localError))

        {

            chatId = broadcastChatId;



            if (chatId.isEmpty() && broadcastId.isNotEmpty())

            {

                juce::String detailError;

                fetchLiveChatIdFromBroadcastId (broadcastId, chatId, detailError);

            }

        }

    }



    if (chatId.isEmpty())

    {

        juce::String activeBroadcastIdLocal;

        juce::String activeChatId;



        if (findLiveChatFromActiveBroadcasts (activeBroadcastIdLocal, activeChatId, localError))

        {

            broadcastId = activeBroadcastIdLocal;

            chatId = activeChatId;

        }

    }



    if (chatId.isEmpty())

    {

        juce::String broadcastChatId;

        juce::String broadcastFromList;



        if (findLiveChatIdFromBroadcastList (broadcastFromList, broadcastChatId, localError))

        {

            broadcastId = broadcastFromList;

            chatId = broadcastChatId;

        }

    }



    if (chatId.isEmpty() && findActiveLiveChatFromLiveVideos (videoId, chatId, localError))

    {

        if (broadcastId.isEmpty())

            broadcastId = videoId;

    }



    if (chatId.isEmpty())

    {

        juce::String broadcastChatId;

        juce::String broadcastFromList;



        if (findBroadcastIdViaAnyLiveBroadcast (broadcastFromList, broadcastChatId, localError))

        {

            if (broadcastId.isEmpty())

                broadcastId = broadcastFromList;

            chatId = broadcastChatId;



            if (chatId.isEmpty() && broadcastId.isNotEmpty())

            {

                juce::String detailError;

                fetchLiveChatIdFromBroadcastId (broadcastId, chatId, detailError);

            }

        }

    }



    if (chatId.isEmpty())

    {

        if (localError.isNotEmpty() && isYoutubeQuotaExceededError (localError))

        {

            apiQuotaExceeded.store (true);

            error = formatYoutubeQuotaExceededMessage();

            return false;

        }



        error = juce::String::fromUTF8 (u8"\u30e9\u30a4\u30d6\u30c1\u30e3\u30c3\u30c8\u3092\u53d6\u5f97\u3067\u304d\u307e\u305b\u3093\u3067\u3057\u305f\u3002"
                                  u8"\u9023\u643a\u30a2\u30ab\u30a6\u30f3\u30c8\u3068\u914d\u4fe1\u30c1\u30e3\u30f3\u30cd\u30eb\u304c\u4e00\u81f4\u3057\u3066\u3044\u308b\u304b\u3001"
                                  u8"Studio \u3067\u30e9\u30a4\u30d6\u30c1\u30e3\u30c3\u30c8\u304c\u6709\u52b9\u304b\u78ba\u8a8d\u3057\u3066\u304f\u3060\u3055\u3044\u3002"
                                  u8"\u89e3\u6c7a\u3057\u306a\u3044\u5834\u5408\u306f\u9023\u643a\u89e3\u9664\u2192\u518d\u9023\u643a\u3092\u8a66\u3057\u3066\u304f\u3060\u3055\u3044\u3002");

        if (localError.isNotEmpty())

            error += "\n" + localError;

        return false;

    }



    {

        const juce::ScopedLock sl (lock);

        liveChatId = chatId;

        if (broadcastId.isNotEmpty())

            activeBroadcastId = broadcastId;

        if (videoId.isNotEmpty())

            activeLiveVideoId = videoId;

        nextPageToken.clear();

    }



    setStatus (juce::String::fromUTF8 (u8"\u30b3\u30e1\u30f3\u30c8\u53d6\u5f97\u4e2d"));

    sendChangeMessage();

    return true;

}



bool YoutubeChatClient::fetchActiveLiveChatId (juce::String& error, const bool requireLiveChatId)

{

    if (! resolveLiveChatSession (error))

        return ! requireLiveChatId;

    return true;

}



void YoutubeChatClient::setActiveStreamKey (const juce::String& streamKey)

{

    const juce::ScopedLock sl (lock);

    activeStreamKey = streamKey.trim();

}



bool YoutubeChatClient::selectBestBroadcastFromItems (const juce::var& items,

                                                    juce::String& broadcastId,

                                                    juce::String& liveChatId) const

{

    if (! items.isArray() || items.getArray()->isEmpty())

        return false;



    juce::String bestId;

    juce::String bestChatId;

    int bestPriority = 1000;



    for (const auto& item : *items.getArray())

    {

        if (! isEndableBroadcast (item))

            continue;



        juce::String candidateId;

        juce::String candidateChatId;



        if (! extractLiveBroadcastFields (item, candidateId, candidateChatId))

            continue;



        const auto priority = endableBroadcastPriority (getBroadcastLifeCycleStatus (item));



        if (priority < bestPriority)

        {

            bestPriority = priority;

            bestId = candidateId;

            bestChatId = candidateChatId;

        }

    }



    if (bestId.isEmpty())

        return false;



    broadcastId = bestId;

    liveChatId = bestChatId;

    return true;

}



bool YoutubeChatClient::selectBestLiveBroadcastFromItems (const juce::var& items,

                                                        juce::String& broadcastId,

                                                        juce::String& liveChatId) const

{

    if (! items.isArray() || items.getArray()->isEmpty())

        return false;



    juce::String bestId;

    juce::String bestChatId;

    int bestPriority = 1000;



    for (const auto& item : *items.getArray())

    {

        if (! isActiveLiveBroadcast (item))

            continue;



        juce::String candidateId;

        juce::String candidateChatId;



        if (! extractLiveBroadcastFields (item, candidateId, candidateChatId))

            continue;



        const auto priority = endableBroadcastPriority (getBroadcastLifeCycleStatus (item));



        if (priority < bestPriority)

        {

            bestPriority = priority;

            bestId = candidateId;

            bestChatId = candidateChatId;

        }

    }



    if (bestId.isEmpty())

        return false;



    broadcastId = bestId;

    liveChatId = bestChatId;

    return true;

}



bool YoutubeChatClient::findBroadcastIdViaStreamKey (juce::String& broadcastId,

                                                   juce::String& liveChatId,

                                                   juce::String& errorOut)

{

    juce::String normalizedKey;



    {

        const juce::ScopedLock sl (lock);

        normalizedKey = StreamEngine::normalizeYoutubeStreamKey (activeStreamKey);

    }



    if (normalizedKey.isEmpty())

        return false;



    const auto streamsResponse = httpGet ("https://www.googleapis.com/youtube/v3/liveStreams?part=cdn,status&mine=true&maxResults=50",

                                          errorOut);



    if (streamsResponse.isEmpty())

        return false;



    const auto streamsJson = juce::JSON::parse (streamsResponse);



    if (! streamsJson.isObject())

        return false;



    auto* streamsRoot = streamsJson.getDynamicObject();



    if (streamsRoot == nullptr || streamsRoot->hasProperty ("error"))

        return false;



    const auto streamItems = streamsRoot->getProperty ("items");



    if (! streamItems.isArray())

        return false;



    juce::StringArray matchedStreamIds;



    for (const auto& item : *streamItems.getArray())

    {

        if (! item.isObject())

            continue;



        if (auto* stream = item.getDynamicObject())

        {

            const auto streamId = stream->getProperty ("id").toString();

            juce::String streamName;



            if (auto* cdn = stream->getProperty ("cdn").getDynamicObject())

            {

                if (auto* ingestionInfo = cdn->getProperty ("ingestionInfo").getDynamicObject())

                    streamName = ingestionInfo->getProperty ("streamName").toString();

            }



            const auto normalizedStreamName = StreamEngine::normalizeYoutubeStreamKey (streamName);



            if (normalizedStreamName.isEmpty() || ! normalizedStreamName.equalsIgnoreCase (normalizedKey))

                continue;



            if (streamId.isNotEmpty())

                matchedStreamIds.addIfNotAlreadyThere (streamId);

        }

    }



    if (matchedStreamIds.isEmpty())

        return false;



    const juce::StringArray broadcastTypes { "all", "persistent", "event" };



    for (const auto& broadcastType : broadcastTypes)

    {

        const auto broadcastsResponse = httpGet ("https://www.googleapis.com/youtube/v3/liveBroadcasts?part=snippet,status,contentDetails"

                                                   "&mine=true&broadcastType=" + broadcastType + "&maxResults=50",

                                                 errorOut);



        if (broadcastsResponse.isEmpty())

            continue;



        const auto broadcastsJson = juce::JSON::parse (broadcastsResponse);



        if (! broadcastsJson.isObject())

            continue;



        auto* broadcastsRoot = broadcastsJson.getDynamicObject();



        if (broadcastsRoot == nullptr || broadcastsRoot->hasProperty ("error"))

            continue;



        const auto broadcastItems = broadcastsRoot->getProperty ("items");



        if (! broadcastItems.isArray())

            continue;



        juce::String bestId;

        juce::String bestChatId;

        int bestPriority = 1000;



        for (const auto& item : *broadcastItems.getArray())

        {

            if (! isEndableBroadcast (item))

                continue;



            juce::String boundStreamId;



            if (auto* broadcast = item.getDynamicObject())

            {

                if (auto* contentDetails = broadcast->getProperty ("contentDetails").getDynamicObject())

                    boundStreamId = contentDetails->getProperty ("boundStreamId").toString();

            }



            if (boundStreamId.isEmpty() || ! matchedStreamIds.contains (boundStreamId))

                continue;



            juce::String candidateId;

            juce::String candidateChatId;



            if (! extractLiveBroadcastFields (item, candidateId, candidateChatId))

                continue;



            const auto priority = endableBroadcastPriority (getBroadcastLifeCycleStatus (item));



            if (priority < bestPriority)

            {

                bestPriority = priority;

                bestId = candidateId;

                bestChatId = candidateChatId;

            }

        }



        if (bestId.isNotEmpty())

        {

            broadcastId = bestId;

            liveChatId = bestChatId;



            if (liveChatId.isEmpty())

            {

                juce::String detailError;

                fetchLiveChatIdFromBroadcastId (broadcastId, liveChatId, detailError);

            }



            return true;

        }

    }



    return false;

}



bool YoutubeChatClient::findBroadcastIdViaAnyLiveBroadcast (juce::String& broadcastId,

                                                            juce::String& liveChatId,

                                                            juce::String& errorOut)

{

    const juce::StringArray broadcastTypes { "all", "persistent", "event" };



    for (const auto& broadcastType : broadcastTypes)

    {

        const auto response = httpGet ("https://www.googleapis.com/youtube/v3/liveBroadcasts?part=snippet,status,contentDetails"

                                       "&mine=true&broadcastType=" + broadcastType + "&maxResults=50",

                                       errorOut);



        if (response.isEmpty())

            continue;



        const auto json = juce::JSON::parse (response);



        if (! json.isObject())

            continue;



        auto* root = json.getDynamicObject();



        if (root == nullptr || root->hasProperty ("error"))

            continue;



        if (selectBestLiveBroadcastFromItems (root->getProperty ("items"), broadcastId, liveChatId))

            return true;



        if (selectBestBroadcastFromItems (root->getProperty ("items"), broadcastId, liveChatId)

            && liveChatId.isNotEmpty())

            return true;

    }



    return false;

}



bool YoutubeChatClient::findBroadcastIdViaActiveStream (juce::String& broadcastId,

                                                        juce::String& liveChatId,

                                                        juce::String& errorOut)

{

    const auto streamsResponse = httpGet ("https://www.googleapis.com/youtube/v3/liveStreams?part=status&mine=true&maxResults=25",

                                          errorOut);



    if (streamsResponse.isEmpty())

        return false;



    const auto streamsJson = juce::JSON::parse (streamsResponse);



    if (! streamsJson.isObject())

        return false;



    auto* streamsRoot = streamsJson.getDynamicObject();



    if (streamsRoot == nullptr || streamsRoot->hasProperty ("error"))

        return false;



    const auto streamItems = streamsRoot->getProperty ("items");



    if (! streamItems.isArray())

        return false;



    juce::StringArray activeStreamIds;



    for (const auto& item : *streamItems.getArray())

    {

        if (! item.isObject())

            continue;



        if (auto* stream = item.getDynamicObject())

        {

            const auto streamId = stream->getProperty ("id").toString();



            if (auto* status = stream->getProperty ("status").getDynamicObject())

            {

                const auto streamStatus = status->getProperty ("streamStatus").toString();



                if (streamStatus == "active" && streamId.isNotEmpty())

                    activeStreamIds.add (streamId);

            }

        }

    }



    if (activeStreamIds.isEmpty())

        return false;



    const juce::StringArray broadcastTypes { "all", "persistent", "event" };



    for (const auto& broadcastType : broadcastTypes)

    {

        const auto broadcastsResponse = httpGet ("https://www.googleapis.com/youtube/v3/liveBroadcasts?part=snippet,status,contentDetails"

                                                   "&mine=true&broadcastType=" + broadcastType + "&maxResults=50",

                                                 errorOut);



        if (broadcastsResponse.isEmpty())

            continue;



        const auto broadcastsJson = juce::JSON::parse (broadcastsResponse);



        if (! broadcastsJson.isObject())

            continue;



        auto* broadcastsRoot = broadcastsJson.getDynamicObject();



        if (broadcastsRoot == nullptr || broadcastsRoot->hasProperty ("error"))

            continue;



        const auto broadcastItems = broadcastsRoot->getProperty ("items");



        if (! broadcastItems.isArray())

            continue;



        juce::String bestId;

        juce::String bestChatId;

        int bestPriority = 1000;



        for (const auto& item : *broadcastItems.getArray())

        {

            if (! isEndableBroadcast (item))

                continue;



            juce::String boundStreamId;



            if (auto* broadcast = item.getDynamicObject())

            {

                if (auto* contentDetails = broadcast->getProperty ("contentDetails").getDynamicObject())

                    boundStreamId = contentDetails->getProperty ("boundStreamId").toString();

            }



            if (boundStreamId.isEmpty() || ! activeStreamIds.contains (boundStreamId))

                continue;



            juce::String candidateId;

            juce::String candidateChatId;



            if (! extractLiveBroadcastFields (item, candidateId, candidateChatId))

                continue;



            const auto priority = endableBroadcastPriority (getBroadcastLifeCycleStatus (item));



            if (priority < bestPriority)

            {

                bestPriority = priority;

                bestId = candidateId;

                bestChatId = candidateChatId;

            }

        }



        if (bestId.isNotEmpty())

        {

            broadcastId = bestId;

            liveChatId = bestChatId;

            return true;

        }

    }



    return false;

}



bool YoutubeChatClient::findBroadcastIdViaLiveVideoSearch (juce::String& broadcastId, juce::String& errorOut)

{

    if (apiQuotaExceeded.load())

        return false;



    juce::String ignoredChatId;

    return findActiveLiveChatFromLiveVideos (broadcastId, ignoredChatId, errorOut);

}



bool YoutubeChatClient::findLiveVideoIdForOwnChannel (juce::String& videoId, juce::String& errorOut) const

{

    videoId.clear();

    juce::String ignoredChatId;

    return findActiveLiveChatFromLiveVideos (videoId, ignoredChatId, errorOut);

}

void YoutubeChatClient::noteYoutubeApiError (juce::String& errorOut, const juce::var& apiError) const

{

    errorOut = extractYoutubeApiErrorMessage (apiError);

    if (isYoutubeQuotaExceededError (errorOut))

        apiQuotaExceeded.store (true);

}

bool YoutubeChatClient::fetchLiveChatIdFromBroadcastId (const juce::String& broadcastId,
                                                        juce::String& liveChatIdOut,
                                                        juce::String& errorOut) const

{

    liveChatIdOut.clear();

    if (broadcastId.isEmpty())

        return false;



    const auto response = httpGet ("https://www.googleapis.com/youtube/v3/liveBroadcasts?part=snippet&id="

                                     + urlEncode (broadcastId),

                                   errorOut);



    if (response.isEmpty())

        return false;



    const auto json = juce::JSON::parse (response);



    if (! json.isObject())

        return false;



    if (auto* root = json.getDynamicObject())

    {

        if (root->hasProperty ("error"))

        {

            noteYoutubeApiError (errorOut, root->getProperty ("error"));

            return false;

        }



        const auto items = root->getProperty ("items");



        if (items.isArray() && ! items.getArray()->isEmpty())

        {

            juce::String ignoredId;

            extractLiveBroadcastFields (items.getArray()->getReference (0), ignoredId, liveChatIdOut);

        }

    }



    return liveChatIdOut.isNotEmpty();

}

bool YoutubeChatClient::fetchLiveChatIdFromVideoId (const juce::String& videoId,
                                                    juce::String& liveChatIdOut,
                                                    juce::String& errorOut) const

{
    liveChatIdOut.clear();

    if (videoId.isEmpty())
        return false;

    const auto response = httpGet ("https://www.googleapis.com/youtube/v3/videos?part=liveStreamingDetails&id="
                                     + urlEncode (videoId),
                                   errorOut);

    if (response.isEmpty())
        return false;

    const auto json = juce::JSON::parse (response);

    if (! json.isObject())
        return false;

    auto* root = json.getDynamicObject();

    if (root == nullptr || root->hasProperty ("error"))
        return false;

    const auto items = root->getProperty ("items");

    if (! items.isArray() || items.getArray()->isEmpty())
        return false;

    if (auto* itemObj = items.getArray()->getReference (0).getDynamicObject())
        if (auto* details = itemObj->getProperty ("liveStreamingDetails").getDynamicObject())
            liveChatIdOut = details->getProperty ("activeLiveChatId").toString();

    return liveChatIdOut.isNotEmpty();
}

bool YoutubeChatClient::resolveBroadcastIdForEnd (juce::String& broadcastId, juce::String& errorOut, const bool forEnding)

{

    {

        const juce::ScopedLock sl (lock);

        if (activeBroadcastId.isNotEmpty())

        {

            broadcastId = activeBroadcastId;

            return true;

        }

    }



    if (! ensureAccessToken (errorOut))

        return false;



    juce::String resolvedChatId;



    if (findBroadcastIdViaStreamKey (broadcastId, resolvedChatId, errorOut)

        || findBroadcastIdViaActiveStream (broadcastId, resolvedChatId, errorOut))

    {

        const juce::ScopedLock sl (lock);

        activeBroadcastId = broadcastId;

        if (resolvedChatId.isNotEmpty())

            liveChatId = resolvedChatId;

        return true;

    }



    if (findBroadcastIdViaLiveVideoSearch (broadcastId, errorOut))

    {

        const juce::ScopedLock sl (lock);

        activeBroadcastId = broadcastId;

        return true;

    }



    const juce::StringArray broadcastTypes { "all", "persistent", "event" };



    for (const auto& broadcastType : broadcastTypes)

    {

        const auto response = httpGet ("https://www.googleapis.com/youtube/v3/liveBroadcasts?part=snippet,status,contentDetails"

                                       "&mine=true&broadcastType=" + broadcastType + "&maxResults=50",

                                       errorOut);



        if (response.isEmpty())

            continue;



        const auto json = juce::JSON::parse (response);



        if (! json.isObject())

            continue;



        auto* root = json.getDynamicObject();



        if (root == nullptr || root->hasProperty ("error"))

            continue;



        juce::String candidateId;

        juce::String candidateChatId;



        if (selectBestBroadcastFromItems (root->getProperty ("items"), candidateId, candidateChatId))

        {

            broadcastId = candidateId;



            {

                const juce::ScopedLock sl (lock);

                activeBroadcastId = broadcastId;

                if (candidateChatId.isNotEmpty())

                    liveChatId = candidateChatId;

            }



            return true;

        }

    }



    if (findBroadcastIdViaAnyLiveBroadcast (broadcastId, resolvedChatId, errorOut))

    {

        const juce::ScopedLock sl (lock);

        activeBroadcastId = broadcastId;

        if (resolvedChatId.isNotEmpty())

            liveChatId = resolvedChatId;

        return true;

    }

    juce::String ownLiveVideoId;

    if (findLiveVideoIdForOwnChannel (ownLiveVideoId, errorOut))

    {

        juce::String chatIdFromVideo;

        juce::String ignoredError;

        juce::String broadcastFromList;

        juce::String chatFromList;



        fetchLiveChatIdFromVideoId (ownLiveVideoId, chatIdFromVideo, ignoredError);



        if (findBroadcastIdViaAnyLiveBroadcast (broadcastFromList, chatFromList, ignoredError))

        {

            broadcastId = broadcastFromList;

            if (chatFromList.isNotEmpty())

                resolvedChatId = chatFromList;

        }

        else

        {

            juce::String lifeCycle;

            if (getBroadcastLifeCycle (ownLiveVideoId, lifeCycle, ignoredError)

                && lifeCycle != "complete"

                && lifeCycle != "revoked")

                broadcastId = ownLiveVideoId;

            else

                broadcastId = ownLiveVideoId;

        }



        const juce::ScopedLock sl (lock);

        activeLiveVideoId = ownLiveVideoId;

        activeBroadcastId = broadcastId;

        if (chatIdFromVideo.isNotEmpty())

            liveChatId = chatIdFromVideo;

        if (resolvedChatId.isEmpty() && chatIdFromVideo.isNotEmpty())

            resolvedChatId = chatIdFromVideo;

        return true;

    }



    errorOut = forEnding

                     ? juce::String::fromUTF8 (u8"\u914d\u4fe1\u4e2d\u306e YouTube \u914d\u4fe1\u3092\u7279\u5b9a\u3067\u304d\u307e\u305b\u3093\u3067\u3057\u305f\u3002"
                                              u8"Drizzle \u306e\u30b9\u30c8\u30ea\u30fc\u30e0\u30ad\u30fc\u304c YouTube Studio \u306e\u914d\u4fe1\u3068\u4e00\u81f4\u3057\u3066\u3044\u308b\u304b\u78ba\u8a8d\u3057\u3001"
                                              u8"Studio \u3067\u624b\u52d5\u300c\u914d\u4fe1\u3092\u7d42\u4e86\u300d\u3057\u3066\u304f\u3060\u3055\u3044\u3002")

                     : juce::String::fromUTF8 (u8"YouTube Studio \u3067\u914d\u4fe1\u3092\u4f5c\u6210\u3057\u3001\u300c\u30b4\u30fc\u30e9\u30a4\u30d6\u300d\u307e\u305f\u306f\u300c\u30c6\u30b9\u30c8\u958b\u59cb\u300d\u3092\u62bc\u3057\u3066\u304b\u3089"
                                              u8" Drizzle \u3067\u914d\u4fe1\u958b\u59cb\u3057\u3066\u304f\u3060\u3055\u3044\u3002");

    return false;

}



bool YoutubeChatClient::isBroadcastAlreadyComplete (const juce::String& broadcastId, juce::String& errorOut) const

{

    if (broadcastId.isEmpty())

        return false;



    const auto response = httpGet ("https://www.googleapis.com/youtube/v3/liveBroadcasts?part=status&id="

                                     + urlEncode (broadcastId),

                                   errorOut);



    if (response.isEmpty())

        return false;



    const auto json = juce::JSON::parse (response);



    if (! json.isObject())

        return false;



    if (auto* root = json.getDynamicObject())

    {

        if (root->hasProperty ("error"))

        {

            errorOut = extractYoutubeApiErrorMessage (root->getProperty ("error"));

            return false;

        }



        const auto items = root->getProperty ("items");



        if (items.isArray() && ! items.getArray()->isEmpty())

            return getBroadcastLifeCycleStatus (items.getArray()->getReference (0)) == "complete";

    }



    return false;

}



bool YoutubeChatClient::getBroadcastLifeCycle (const juce::String& broadcastId,

                                             juce::String& lifeCycleOut,

                                             juce::String& errorOut) const

{

    lifeCycleOut = {};



    if (broadcastId.isEmpty())

        return false;



    const auto response = httpGet ("https://www.googleapis.com/youtube/v3/liveBroadcasts?part=status&id="

                                     + urlEncode (broadcastId),

                                   errorOut);



    if (response.isEmpty())

        return false;



    const auto json = juce::JSON::parse (response);



    if (! json.isObject())

        return false;



    if (auto* root = json.getDynamicObject())

    {

        if (root->hasProperty ("error"))

        {

            errorOut = extractYoutubeApiErrorMessage (root->getProperty ("error"));

            return false;

        }



        const auto items = root->getProperty ("items");



        if (items.isArray() && ! items.getArray()->isEmpty())

        {

            lifeCycleOut = getBroadcastLifeCycleStatus (items.getArray()->getReference (0));

            return lifeCycleOut.isNotEmpty();

        }

    }



    return false;

}



bool YoutubeChatClient::transitionBroadcastStatus (const juce::String& broadcastId,

                                                   const juce::String& targetStatus,

                                                   juce::String& errorOut)

{

    if (broadcastId.isEmpty())

    {

        errorOut = juce::String::fromUTF8 (u8"\u914d\u4fe1 ID \u304c\u7a7a\u3067\u3059\u3002");

        return false;

    }



    const auto url = juce::String ("https://www.googleapis.com/youtube/v3/liveBroadcasts/transition")

                     + "?id=" + urlEncode (broadcastId)

                     + "&broadcastStatus=" + urlEncode (targetStatus)

                     + "&part=id,status";



    const auto response = httpPostBearer (url, errorOut);



    if (response.isEmpty())

    {

        if (errorOut.isEmpty())

            errorOut = juce::String::fromUTF8 (u8"YouTube API \u304b\u3089\u5fdc\u7b54\u304c\u3042\u308a\u307e\u305b\u3093\u3067\u3057\u305f\u3002");

        return false;

    }



    const auto json = juce::JSON::parse (response);

    const juce::String apiErrorKey ("error");



    if (json.isObject())

    {

        if (auto* root = json.getDynamicObject())

        {

            if (root->hasProperty (apiErrorKey))

            {

                errorOut = extractYoutubeApiErrorMessage (root->getProperty (apiErrorKey));

                return false;

            }

        }

    }



    return true;

}



bool YoutubeChatClient::beginActiveLiveBroadcast (juce::String& errorOut)

{

    if (! isAuthenticated())

    {

        errorOut = juce::String::fromUTF8 (u8"YouTube \u672a\u9023\u643a\u306e\u305f\u3081\u3001\u914d\u4fe1\u3092\u958b\u59cb\u3067\u304d\u307e\u305b\u3093\u3002");

        return false;

    }



    if (! ensureAccessToken (errorOut))

        return false;



    juce::Thread::sleep (5000);



    for (int attempt = 0; attempt < 25; ++attempt)

    {

        if (attempt > 0)

            juce::Thread::sleep (1500);



        juce::String broadcastId;



        if (! resolveBroadcastIdForEnd (broadcastId, errorOut, false))

            continue;



        juce::String lifeCycle;



        if (! getBroadcastLifeCycle (broadcastId, lifeCycle, errorOut))

            continue;



        if (lifeCycle == "live")

        {

            juce::String ignoredChatError;

            resolveLiveChatSession (ignoredChatError);

            setStatus (juce::String::fromUTF8 (u8"\u30b3\u30e1\u30f3\u30c8\u53d6\u5f97\u4e2d"));

            sendChangeMessage();

            return true;

        }



        if (lifeCycle == "liveStarting")

            continue;



        if (lifeCycle == "complete" || lifeCycle == "revoked")

        {

            errorOut = juce::String::fromUTF8 (u8"\u7d42\u4e86\u6e08\u307f\u306e\u914d\u4fe1\u3067\u3059\u3002\u65b0\u3057\u3044\u914d\u4fe1\u3092 YouTube Studio \u3067\u4f5c\u6210\u3057\u3066\u304f\u3060\u3055\u3044\u3002");

            return false;

        }



        juce::String transitionError;



        if (transitionBroadcastStatus (broadcastId, "live", transitionError))

            continue;



        if (lifeCycle == "ready" || lifeCycle == "created" || lifeCycle == "testing")

        {

            juce::String ignoredError;

            transitionBroadcastStatus (broadcastId, "testing", ignoredError);

            if (transitionBroadcastStatus (broadcastId, "live", transitionError))

                continue;

        }



        errorOut = transitionError.isNotEmpty()

                       ? transitionError

                       : juce::String::fromUTF8 (u8"YouTube \u914d\u4fe1\u3092 live \u72b6\u614b\u306b\u3067\u304d\u307e\u305b\u3093\u3067\u3057\u305f\u3002");

    }



    if (errorOut.isEmpty())

        errorOut = juce::String::fromUTF8 (u8"YouTube \u914d\u4fe1\u3092 live \u72b6\u614b\u306b\u3067\u304d\u307e\u305b\u3093\u3067\u3057\u305f\u3002"
                                          u8"RTMP \u63a5\u7d9a\u5f8c\u3001Studio \u3067\u300c\u30b4\u30fc\u30e9\u30a4\u30d6\u300d\u3092\u78ba\u8a8d\u3057\u3066\u304f\u3060\u3055\u3044\u3002");



    return false;

}



void YoutubeChatClient::beginActiveLiveBroadcastAsync (std::function<void (bool, const juce::String&)> onFinished)

{

    if (startBroadcastThread != nullptr)

    {

        startBroadcastThread->stopThread (1000);

        startBroadcastThread.reset();

    }



    startBroadcastThread = std::make_unique<StartBroadcastThread> (*this, std::move (onFinished));

    startBroadcastThread->startThread();

}



void YoutubeChatClient::warmUpLiveSessionCacheAsync()

{

    if (apiQuotaExceeded.load())

        return;



    std::thread ([this]

    {

        juce::String error;

        resolveLiveChatSession (error);

    }).detach();

}



void YoutubeChatClient::cancelPendingBroadcastOperations()

{

    if (startBroadcastThread != nullptr)

    {

        startBroadcastThread->stopThread (1000);

        startBroadcastThread.reset();

    }



    if (endBroadcastThread != nullptr)

    {

        endBroadcastThread->stopThread (1000);

        endBroadcastThread.reset();

    }

}



void YoutubeChatClient::resetStreamSessionState()

{

    chatConnectFailCount.store (0);

    {

        const juce::ScopedLock sl (lock);

        liveChatId.clear();

        activeBroadcastId.clear();

        activeLiveVideoId.clear();

        nextPageToken.clear();

        messages.clear();

        seenMessageIds.clear();

    }



    if (isAuthenticated())

        setStatus (juce::String::fromUTF8 (u8"YouTube \u9023\u643a\u6e08\u307f"));

    else

        setStatus (juce::String::fromUTF8 (u8"\u672a\u9023\u643a"));

}



void YoutubeChatClient::setChatConnectWaitingStatus()

{

    setStatus (juce::String::fromUTF8 (u8"YouTube Studio \u306e\u30e9\u30a4\u30d6\u958b\u59cb\u3092\u5f85\u6a5f\u4e2d..."));

}



void YoutubeChatClient::setChatConnectRetryingStatus()

{

    setStatus (juce::String::fromUTF8 (u8"\u30c1\u30e3\u30c3\u30c8\u63a5\u7d9a\u3092\u8a66\u884c\u4e2d..."));

}



bool YoutubeChatClient::endActiveLiveBroadcast (juce::String& errorOut)

{

    auto markBroadcastEnded = [this]() -> bool

    {

        {

            const juce::ScopedLock sl (lock);

            liveChatId.clear();

            activeBroadcastId.clear();

            activeLiveVideoId.clear();

            activeStreamKey.clear();

            nextPageToken.clear();

        }



        setStatus (juce::String::fromUTF8 (u8"YouTube \u9023\u643a\u6e08\u307f"));

        sendChangeMessage();

        return true;

    };



    if (! isAuthenticated())

    {

        errorOut = juce::String::fromUTF8 (u8"YouTube \u672a\u9023\u643a\u306e\u305f\u3081\u3001\u914d\u4fe1\u3092\u81ea\u52d5\u7d42\u4e86\u3067\u304d\u307e\u305b\u3093\u3002");

        return false;

    }



    if (! ensureAccessToken (errorOut))

        return false;



    juce::String broadcastId;



    if (! resolveBroadcastIdForEnd (broadcastId, errorOut))

    {

        juce::String ignoredChatId;

        if (! findBroadcastIdViaAnyLiveBroadcast (broadcastId, ignoredChatId, errorOut))

            return false;

    }



    if (isBroadcastAlreadyComplete (broadcastId, errorOut))

        return markBroadcastEnded();



    const auto url = juce::String ("https://www.googleapis.com/youtube/v3/liveBroadcasts/transition")

                     + "?id=" + urlEncode (broadcastId)

                     + "&broadcastStatus=complete"

                     + "&part=id,status,contentDetails";



    for (int attempt = 0; attempt < 3; ++attempt)

    {

        if (attempt > 0)

            juce::Thread::sleep (1000);



        juce::String attemptError;

        const auto response = httpPostBearer (url, attemptError);



        if (response.isEmpty())

        {

            errorOut = attemptError.isNotEmpty()

                           ? attemptError

                           : juce::String::fromUTF8 (u8"YouTube API \u304b\u3089\u5fdc\u7b54\u304c\u3042\u308a\u307e\u305b\u3093\u3067\u3057\u305f\u3002");

            continue;

        }



        const auto json = juce::JSON::parse (response);

        const juce::String apiErrorKey ("error");



        if (json.isObject())

        {

            if (auto* root = json.getDynamicObject())

            {

                if (root->hasProperty (apiErrorKey))

                {

                    const auto apiMessage = extractYoutubeApiErrorMessage (root->getProperty (apiErrorKey));

                    bool alreadyEnded = false;



                    if (auto* errObj = root->getProperty (apiErrorKey).getDynamicObject())

                    {

                        if (auto* errors = errObj->getProperty ("errors").getArray())

                        {

                            for (const auto& item : *errors)

                            {

                                if (auto* reasonObj = item.getDynamicObject())

                                {

                                    const auto reason = reasonObj->getProperty ("reason").toString();



                                    if (reason == "redundantTransition"

                                        || reason == "liveBroadcastInactive"

                                        || reason == "invalidTransition")

                                    {

                                        alreadyEnded = isBroadcastAlreadyComplete (broadcastId, attemptError);

                                        break;

                                    }

                                }

                            }

                        }

                    }



                    if (alreadyEnded)

                        return markBroadcastEnded();



                    errorOut = apiMessage.isNotEmpty()

                                   ? apiMessage

                                   : juce::String::fromUTF8 (u8"YouTube \u914d\u4fe1\u306e\u7d42\u4e86\u306b\u5931\u6557\u3057\u307e\u3057\u305f\u3002");

                    continue;

                }



                return markBroadcastEnded();

            }

        }



        errorOut = juce::String::fromUTF8 (u8"YouTube \u914d\u4fe1\u306e\u7d42\u4e86\u5fdc\u7b54\u306e\u89e3\u6790\u306b\u5931\u6557\u3057\u307e\u3057\u305f\u3002");

    }



    return false;

}



void YoutubeChatClient::endActiveLiveBroadcastAsync (std::function<void (bool, const juce::String&)> onFinished)

{

    if (endBroadcastThread != nullptr)

    {

        endBroadcastThread->stopThread (1000);

        endBroadcastThread.reset();

    }



    endBroadcastThread = std::make_unique<EndBroadcastThread> (*this, std::move (onFinished));

    endBroadcastThread->startThread();

}



bool YoutubeChatClient::pollChatOnce (juce::String& error)

{

    juce::String chatId;

    juce::String pageToken;



    {

        const juce::ScopedLock sl (lock);

        chatId = liveChatId;

        pageToken = nextPageToken;

    }



    if (chatId.isEmpty() && ! resolveLiveChatSession (error))

        return false;



    {

        const juce::ScopedLock sl (lock);

        chatId = liveChatId;

        pageToken = nextPageToken;

    }



    auto url = "https://www.googleapis.com/youtube/v3/liveChat/messages?liveChatId="

             + urlEncode (chatId)

             + "&part=snippet,authorDetails&maxResults=200";



    if (pageToken.isNotEmpty())

        url += "&pageToken=" + urlEncode (pageToken);



    const auto response = httpGet (url, error);



    if (response.isEmpty())

        return false;



    const auto json = juce::JSON::parse (response);



    if (! json.isObject())

    {

        error = response;

        return false;

    }



    auto* root = json.getDynamicObject();



    if (root == nullptr)

        return false;



    if (root->hasProperty ("error"))

    {

        error = extractYoutubeApiErrorMessage (root->getProperty ("error"));

        {

            const juce::ScopedLock sl (lock);

            liveChatId.clear();

            nextPageToken.clear();

        }

        return false;

    }



    pollingIntervalMs = juce::jlimit (1000, 10000, (int) root->getProperty ("pollingIntervalMillis"));



    const auto nextToken = root->getProperty ("nextPageToken").toString();

    const auto items = root->getProperty ("items");



    if (items.isArray())

    {

        for (const auto& item : *items.getArray())

        {

            if (! item.isObject())

                continue;



            auto* obj = item.getDynamicObject();

            YoutubeChatMessage message;

            message.id = obj->getProperty ("id").toString();

            if (auto* author = obj->getProperty ("authorDetails").getDynamicObject())

            {

                message.author = author->getProperty ("displayName").toString();

                message.isOwner = (bool) author->getProperty ("isChatOwner");

            }



            if (auto* snippet = obj->getProperty ("snippet").getDynamicObject())

            {

                message.text = snippet->getProperty ("displayMessage").toString();



                if (message.text.isEmpty())

                {

                    if (auto* textDetails = snippet->getProperty ("textMessageDetails").getDynamicObject())

                        message.text = textDetails->getProperty ("messageText").toString();

                }

            }



            if (message.text.isNotEmpty())

                appendMessage (message);

        }

    }



    {

        const juce::ScopedLock sl (lock);

        nextPageToken = nextToken;

    }



    updateConcurrentViewers();

    return true;

}



void YoutubeChatClient::updateConcurrentViewers()

{

    juce::String broadcastId;

    {

        const juce::ScopedLock sl (lock);

        broadcastId = activeBroadcastId;

    }



    if (broadcastId.isEmpty())

        return;



    juce::String error;

    const auto response = httpGet ("https://www.googleapis.com/youtube/v3/videos?part=liveStreamingDetails&id="

                                   + urlEncode (broadcastId),

                                   error);



    if (response.isEmpty())

        return;



    const auto json = juce::JSON::parse (response);



    if (! json.isObject())

        return;



    auto* root = json.getDynamicObject();



    if (root == nullptr)

        return;



    const auto items = root->getProperty ("items");



    if (! items.isArray() || items.getArray()->isEmpty())

        return;



    const auto firstItem = items.getArray()->getFirst();

    juce::var viewers;



    if (auto* details = firstItem.getProperty ("liveStreamingDetails", {}).getDynamicObject())

        viewers = details->getProperty ("concurrentViewers");



    if (viewers.isString() || viewers.isInt() || viewers.isInt64())

        concurrentViewers.store (viewers.toString().getIntValue());

}



bool YoutubeChatClient::postChatMessage (const juce::String& text, juce::String& error)

{

    juce::String chatId;

    juce::String bearer;



    {

        const juce::ScopedLock sl (lock);

        chatId = liveChatId;

        bearer = tokens.accessToken;

    }



    if (chatId.isEmpty() && ! fetchActiveLiveChatId (error))

        return false;



    {

        const juce::ScopedLock sl (lock);

        chatId = liveChatId;

        bearer = tokens.accessToken;

    }



    juce::DynamicObject::Ptr snippet (new juce::DynamicObject());

    snippet->setProperty ("type", "textMessageEvent");

    juce::DynamicObject::Ptr textDetails (new juce::DynamicObject());

    textDetails->setProperty ("messageText", text);

    snippet->setProperty ("textMessageDetails", juce::var (textDetails.get()));



    juce::DynamicObject::Ptr body (new juce::DynamicObject());

    body->setProperty ("snippet", juce::var (snippet.get()));



    const auto jsonBody = juce::JSON::toString (juce::var (body.get()));

    const auto url = "https://www.googleapis.com/youtube/v3/liveChat/messages?part=snippet&liveChatId="

                   + urlEncode (chatId);

    const auto response = httpPostJson (url, jsonBody, bearer, error);



    if (response.isEmpty())

        return false;



    const auto json = juce::JSON::parse (response);



    if (json.isObject())

    {

        if (auto* obj = json.getDynamicObject())

        {

            if (obj->hasProperty ("error"))

            {

                error = extractYoutubeApiErrorMessage (obj->getProperty ("error"));

                return false;

            }

        }

    }



    return true;

}



void YoutubeChatClient::beginOAuthFlow (std::function<void (bool, const juce::String&)> onFinished)

{

    if (! hasApiCredentials())

    {

        if (onFinished != nullptr)

            onFinished (false, juce::String::fromUTF8 (u8"Client ID / Client Secret \u304c\u672a\u8a2d\u5b9a\u3067\u3059\u3002"));



        return;

    }



    if (oauthThread != nullptr)

    {

        oauthThread->stopThread (1000);

        oauthThread.reset();

    }



    const auto state = generateStateToken();

    const auto authUrl = buildAuthUrl (state);

    oauthThread = std::make_unique<OAuthLoopbackThread> (getApiConfig().redirectPort, state);

    oauthThread->startThread();



    if (! juce::URL (authUrl).launchInDefaultBrowser())

    {

        oauthThread->stopThread (1000);

        oauthThread.reset();



        if (onFinished != nullptr)

            onFinished (false, juce::String::fromUTF8 (u8"\u30d6\u30e9\u30a6\u30b6\u3092\u958b\u3051\u3089\u308c\u307e\u305b\u3093\u3067\u3057\u305f\u3002"));



        return;

    }



    auto* oauthListener = static_cast<OAuthLoopbackThread*> (oauthThread.get());

    std::thread ([this, onFinished, oauthListener]()

    {

        oauthListener->waitForThreadToExit (120000);

        const auto code = oauthListener->getAuthCode();

        const auto threadError = oauthListener->getError();



        juce::MessageManager::callAsync ([this, onFinished, code, threadError]()

        {

            oauthThread.reset();

            juce::String error;



            if (code.isEmpty())

            {

                if (onFinished != nullptr)

                    onFinished (false, threadError.isNotEmpty() ? threadError

                                                                : juce::String::fromUTF8 (u8"OAuth \u306b\u5931\u6557\u3057\u307e\u3057\u305f\u3002"));

                return;

            }



            if (! exchangeAuthCode (code, error))

            {

                if (onFinished != nullptr)

                    onFinished (false, error);

                return;

            }



            setStatus (juce::String::fromUTF8 (u8"YouTube \u9023\u643a\u6e08\u307f"));



            if (onFinished != nullptr)

                onFinished (true, {});

        });

    }).detach();

}



void YoutubeChatClient::startPolling()

{

    if (pollingActive.load())

        return;



    if (! isAuthenticated())

        return;



    stopPollRequested.store (false);

    pollingActive.store (true);

    chatConnectFailCount.store (0);

    lastChatResolveAttemptMs.store (0);

    chatPollStartMs.store ((int64_t) juce::Time::getMillisecondCounterHiRes());



    {

        const juce::ScopedLock sl (lock);

        messages.clear();

        seenMessageIds.clear();

        nextPageToken.clear();

    }



    pollThread = std::make_unique<ChatPollThread> (*this);

    pollThread->startThread();

    setChatConnectRetryingStatus();

}



void YoutubeChatClient::stopPolling()

{

    stopPollRequested.store (true);



    if (pollThread != nullptr)

    {

        pollThread->stopThread (5000);

        pollThread.reset();

    }



    pollingActive.store (false);

    chatConnectFailCount.store (0);



    if (isAuthenticated())

        setStatus (juce::String::fromUTF8 (u8"YouTube \u9023\u643a\u6e08\u307f"));

    else

        setStatus (juce::String::fromUTF8 (u8"\u672a\u9023\u643a"));

}



void YoutubeChatClient::runPollLoop()

{

    while (! stopPollRequested.load())

    {

        juce::String error;



        if (! ensureAccessToken (error))

        {

            setStatus (error);

            break;

        }



        if (! pollChatOnce (error))

        {

            if (apiQuotaExceeded.load())

            {

                setStatus (formatYoutubeQuotaExceededMessage());

                break;

            }



            if (error.isEmpty())

            {

                setChatConnectRetryingStatus();

            }

            else

            {

                const auto failCount = chatConnectFailCount.fetch_add (1) + 1;

                const auto elapsedMs = (int64_t) juce::Time::getMillisecondCounterHiRes() - chatPollStartMs.load();

                const bool showHardError = failCount >= kChatConnectGraceAttempts || elapsedMs >= 45000;



                if (showHardError)

                    setStatus (error.isNotEmpty() ? error

                                                  : juce::String::fromUTF8 (u8"\u30e9\u30a4\u30d6\u30c1\u30e3\u30c3\u30c8\u3092\u53d6\u5f97\u3067\u304d\u307e\u305b\u3093\u3067\u3057\u305f\u3002"));

                else

                    setChatConnectRetryingStatus();

            }

        }

        else

        {

            chatConnectFailCount.store (0);

            setStatus (juce::String::fromUTF8 (u8"\u30b3\u30e1\u30f3\u30c8\u53d6\u5f97\u4e2d"));

        }



        const int waitMs = juce::jmax (1000, pollingIntervalMs);



        for (int elapsed = 0; elapsed < waitMs && ! stopPollRequested.load(); elapsed += 100)

            juce::Thread::sleep (100);

    }



    pollingActive.store (false);

}



void YoutubeChatClient::sendChatMessage (const juce::String& text)

{

    const auto trimmed = text.trim();



    if (trimmed.isEmpty())

        return;



    juce::String error;



    if (! ensureAccessToken (error))

    {

        setStatus (error);

        return;

    }



    if (! postChatMessage (trimmed, error))

        setStatus (error.isNotEmpty() ? error

                                      : juce::String::fromUTF8 (u8"\u30b3\u30e1\u30f3\u30c8\u9001\u4fe1\u306b\u5931\u6557\u3057\u307e\u3057\u305f\u3002"));

}



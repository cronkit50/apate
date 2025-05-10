#ifndef OPENAI_HPP
#define OPENAI_HPP

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <condition_variable>
#include <future>
#include <string>
#include <mutex>
#include <queue>
#include <thread>

namespace openai{

enum Role{
    ROLE_USER,
    ROLE_ASSISTANT,
};

struct chatGPTMessage{
    Role role;
    std::string message;
};


struct chatGPTModel{
    std::string modelValue;
};

struct chatGPTPrompt{
    chatGPTModel model;
    std::string  systemPrompt;

    // oldest messages first.
    std::vector<chatGPTMessage> history;
    std::string request;

    nlohmann::json JsonRequest(void) const;
};

struct chatGPTResponse{
    CURLcode HTTPCode = CURLE_OK;
};

struct chatGPTDispatchRequest{
    std::promise<chatGPTResponse> promise;
    nlohmann::json                json;
};

class chatGPT{
public:
    chatGPT() = default;
    chatGPT(chatGPT&) = delete;
    chatGPT(chatGPT&&) = delete;
    chatGPT& operator=(const chatGPT&)  = delete;
    chatGPT& operator=(const chatGPT&&) = delete;

    chatGPT(const std::string_view& openAIKey);
    ~chatGPT();

    std::future<chatGPTResponse> AskChatGPTAsync (const chatGPTPrompt& prompt);

private:

    void HandleQueue(void);

    std::string m_openAI_Key;

    CURL*       m_curl = nullptr;

    // to do, use a thread pool?
    std::thread                      m_dispatcher;
    std::condition_variable          m_dispatchQCV;
    std::queue<chatGPTDispatchRequest> m_dispatchQ;
    std::mutex                       m_dispatchQMtx;

    std::atomic<bool>                m_shutDown = false;
};
}
#endif
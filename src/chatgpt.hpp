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
#include <variant>

namespace openai{

enum Role{
    ROLE_USER,
    ROLE_ASSISTANT,
};

enum OutputItemType{
    OUTPUT_MESSAGE,
    OUTPUT_FILE_SEARCH,
    OUTPUT_FUNCTION_TOOL,
    OUTPUT_WEB_SEARCH,
    OUTPUT_COMPUTER_TOOL,
    OUTPUT_REASONING
};


struct chatGPTMessage{
    Role role;
    std::string message;
};


struct chatGPTModel{
    chatGPTModel() = default;
    chatGPTModel(std::string_view& view) : modelValue(view){
    }

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


struct chatGPTOutputMessage{
    bool refused = false;

    std::string id;
    std::string message;

};


typedef std::variant<chatGPTOutputMessage> chatGptOutputVariant;

struct chatGPTOutputItem{
    OutputItemType       outputType;
    chatGptOutputVariant content;
};

struct chatGPTResponse{

    CURLcode HTTPCode = CURLE_OK;
    bool responseOK   = false;
    std::string status;
    std::string responseFailureReason;

    std::string id;
    std::vector<chatGPTOutputItem> outputs;

    // usage factors
    size_t inputToken = 0;
    size_t outputTokens = 0;
    size_t totalTokens = 0;

    void Put(const nlohmann::json& json);

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
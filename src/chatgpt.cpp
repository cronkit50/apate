#include "chatgpt.hpp"

#include <nlohmann/json.hpp>

static const char *openAI_API_URL = "https://api.openai.com/v1/chat/completions";

static size_t CurlWriteToString(void* contents, size_t size, size_t nmemb, std::string* response) {
    const size_t totalSize = size * nmemb;
    response->append((char*)contents, totalSize);
    return totalSize;
}

namespace openai
{
nlohmann::json chatGPTPrompt::JsonRequest(void) const{
    nlohmann::json json;
    json["model"] = model.modelValue;
    json["messages"][0]["role"] = "system";
    json["messages"][0]["content"] = systemPrompt;

    for (size_t ii = 0; ii < history.size(); ii++){

        const auto &msg = history[ii];
        json["messages"][ii + 1]["role"] = (ROLE_USER == msg.role) ? "user" : "assistant";
        json["messages"][ii + 1]["content"] = msg.message;
    }

    size_t lastMessage = json["messages"].size();
    json["messages"][lastMessage]["role"]    = "user";
    json["messages"][lastMessage]["content"] = request;

    return json;
}

chatGPT::chatGPT(const std::string_view& openAIKey) : m_openAI_Key(openAIKey){
    m_curl = curl_easy_init();
}
chatGPT::~chatGPT(){
    std::unique_lock lock (m_dispatchQMtx);
    m_shutDown = true;
    m_dispatchQCV.notify_all();
    lock.unlock();

    if (m_dispatcher.joinable()){
        m_dispatcher.join();
    }

    if (nullptr != m_curl){
        curl_easy_cleanup(m_curl);
    }
}

std::future<chatGPTResponse> chatGPT::AskChatGPTAsync(const chatGPTPrompt& prompt){
    std::lock_guard lock(m_dispatchQMtx);

    chatGPTDispatchRequest toDispatch;
    toDispatch.json    = prompt.JsonRequest();
    auto future        = toDispatch.promise.get_future();

    m_dispatchQ.push(std::move(toDispatch));
    m_dispatchQCV.notify_all();

    return future;
}
void chatGPT::HandleQueue(void){
    std::unique_lock lock(m_dispatchQMtx);

    while(true){
        m_dispatchQCV.wait(lock, [&](){return (m_shutDown || !m_dispatchQ.empty());});

        if (m_shutDown){
            // clear out the queue and send any threads waiting on a promise
            // a status code.

            while(!m_dispatchQ.empty()){
                chatGPTDispatchRequest request (std::move (m_dispatchQ.front()));
                chatGPTResponse        response;

                response.HTTPCode = CURLE_RECV_ERROR;
                request.promise.set_value(response);

                m_dispatchQ.pop();
            }
            return;
        }

        // handle normally
        chatGPTDispatchRequest request (std::move (m_dispatchQ.front()));
        m_dispatchQ.pop();
        lock.unlock();


        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("Authorization: Bearer " + m_openAI_Key).c_str());

        std::string requestDataStr = request.json.dump();
        std::string curlResponse;

        curl_easy_setopt(m_curl, CURLOPT_URL, openAI_API_URL);
        curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, requestDataStr.c_str ());
        curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE, requestDataStr.size ());
        curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, CurlWriteToString);
        curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &curlResponse);
        curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0);

        chatGPTResponse chatGPTResponse;
        chatGPTResponse.HTTPCode = curl_easy_perform(m_curl);

        if((chatGPTResponse.HTTPCode = curl_easy_perform(m_curl))!=CURLE_OK){
            // nothing to do. Just return the error code.
            request.promise.set_value(std::move(chatGPTResponse));
            continue;
        }

        // to do
        request.promise.set_value(std::move(chatGPTResponse));

    }
}

}
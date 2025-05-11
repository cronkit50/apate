#include "chatgpt.hpp"

#include "log/log.hpp"

#include <nlohmann/json.hpp>

#include <format>

static const char *openAI_API_URL = "https://api.openai.com/v1/responses";


static size_t CurlWriteToString(void* contents, size_t size, size_t nmemb, std::string* response) {
    const size_t totalSize = size * nmemb;
    response->append((char*)contents, totalSize);
    return totalSize;
}


static openai::OutputItemType ChatGPTMessageTypeStrToType(const std::string_view &view){
    static const std::map<std::string, openai::OutputItemType> dict{
        { "message"         , openai::OUTPUT_MESSAGE },
        { "file_search_call", openai::OUTPUT_FILE_SEARCH },
        { "function_call"   , openai::OUTPUT_FUNCTION_TOOL },
        { "web_search_call" , openai::OUTPUT_WEB_SEARCH },
        { "reasoning"       , openai::OUTPUT_REASONING },
    };

    if (view.empty()){
        throw std::runtime_error("Empty string");
    }

    std::string lowerCase;
    std::transform(view.begin(), view.end(), std::back_inserter(lowerCase),
                   [](unsigned char c){ return std::tolower(c); });

    return dict.at(lowerCase);
}

static openai::chatGPTOutputItem OutputItemFromJson(const nlohmann::json &outputBlock){

    openai::chatGPTOutputItem item;
    item.outputType = ChatGPTMessageTypeStrToType(outputBlock["type"]);

    switch(item.outputType){
        case openai::OUTPUT_MESSAGE:
        {
            openai::chatGPTOutputMessage message;

            message.id = outputBlock["id"];

            if (outputBlock["content"][0].count ("refusal") > 0){
                message.message = outputBlock["content"][0]["refusal"];
                message.refused = true;
            }
            else{
                message.message = outputBlock["content"][0]["text"];
                // to do citations
            }

            item.content = message;
            break;
        }
        case openai::OUTPUT_REASONING:
        {
            openai::chatGPTOutputReasoning reasoning;
            reasoning.id = outputBlock["id"];
            reasoning.summary = outputBlock["summary"]["text"];

            item.content = reasoning;
            break;
        }
        default:
        {
            // to do
            std::string msg = std::format("unimplemented chatgpt response type: {}", std::string (outputBlock["type"]));
            throw std::runtime_error (msg);

            break;
        }
    }
    return item;
}


namespace openai
{
nlohmann::json chatGPTPrompt::JsonRequest(void) const{
    nlohmann::json json;
    json["model"] = model.modelValue;
    json["instructions"] = systemPrompt;

    for (size_t ii = 0; ii < history.size(); ii++){

        const auto &msg = history[ii];
        json["input"][ii + 1]["role"] = (ROLE_USER == msg.role) ? "user" : "assistant";
        json["input"][ii + 1]["content"] = msg.message;
    }

    size_t lastMessage = json["input"].size();
    json["input"][lastMessage]["role"]    = "user";
    json["input"][lastMessage]["content"] = request;

    return json;
}


void chatGPTResponse::Put(const nlohmann::json& json){
    if(json.count("id") > 0){
        id = json["id"];
    }

    if(json.count("status") > 0){
        status = json["status"];
    }

    if(json.count("created_at") > 0){
        createdAt = json["created_at"];
    }

    if(json.count("error") > 0){
        std::string errorCode;
        std::string errorReason;

        try{
            errorCode   = json["error"]["code"];
        }
        catch (...){
            // failed...
        }
        try{
            errorReason  = json["error"]["reason"];
        }
        catch (...){
            // failed...
        }
        if (!errorCode.empty() || !errorReason.empty()){
            responseFailureReason = std::format("ERROR CODE {} - {}",
                                                errorCode.empty() ? "NONE" : errorCode,
                                                errorReason);

            responseOK = false;
        }


    }

    responseOK = (status == "completed" && responseFailureReason.empty());

    if(!json.count("output")){
        // no output objects
    }
    else{
        auto &jsonOutputs = json["output"];
        for(size_t ii = 0; ii < jsonOutputs.size(); ii++){
            try{
                chatGPTOutputItem item = OutputItemFromJson(jsonOutputs[ii]);
                outputs.push_back(std::move(item));
            }
            catch (...){
                // throw away this output
            }
        }
    }
}

chatGPT::chatGPT(const std::string_view& openAIKey) : m_openAI_Key(openAIKey){
    m_curl = curl_easy_init();
    m_dispatcher = std::thread(&chatGPT::HandleQueue, this);
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
        if((chatGPTResponse.HTTPCode = curl_easy_perform(m_curl)) != CURLE_OK){
            // nothing to do. Just return the error code.
            request.promise.set_value(std::move(chatGPTResponse));

        }
        else{
            nlohmann::json jsonResponse = nlohmann::json::parse(curlResponse);
            try{
                chatGPTResponse.Put(jsonResponse);
            }
            catch (...){
                chatGPTResponse.responseOK = false;
            }

            request.promise.set_value(std::move(chatGPTResponse));

            if (NULL != headers){
                curl_slist_free_all (headers);
            }
        }
    }
}


}
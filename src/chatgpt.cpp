#include "chatgpt.hpp"

#include "common/util.hpp"
#include "log/log.hpp"

#include <nlohmann/json.hpp>

#include <format>


namespace openai
{

static const char *openAI_API_URL = "https://api.openai.com/v1/responses";


static size_t CurlWriteToString(void* contents, size_t size, size_t nmemb, std::string* response) {
    const size_t totalSize = size * nmemb;
    response->append((char*)contents, totalSize);
    return totalSize;
}


static OutputItemType ChatGPTMessageTypeStrToType(const std::string_view &typeStr){
    if(typeStr=="message"){
        return OutputItemType::OUTPUT_MESSAGE;
    }

    if(typeStr=="file_search_call"){
        return OutputItemType::OUTPUT_FILE_SEARCH;
    }

    if(typeStr=="function_call"){
        return OutputItemType::OUTPUT_FUNCTION_TOOL;
    }

    if(typeStr=="web_search_call"){
        return OutputItemType::OUTPUT_WEB_SEARCH;
    }

    if(typeStr=="reasoning"){
        return OutputItemType::OUTPUT_REASONING;
    }

    return OutputItemType::OUTPUT_UNKNOWN;
}

static openai::chatGPTOutputItem OutputItemFromJson(const nlohmann::json &outputBlock){

    openai::chatGPTOutputItem item;

    if (!outputBlock.contains("type")) {
        APATE_LOG_WARN_AND_THROW(std::invalid_argument,"Missing 'type' in output block");
    }

    item.outputType = ChatGPTMessageTypeStrToType(outputBlock["type"]);

    switch(item.outputType){
        case OutputItemType::OUTPUT_MESSAGE:
        {
            openai::chatGPTOutputMessage message;

            if (outputBlock.contains("content") && outputBlock["content"].is_array() && !outputBlock["content"].empty()){
                const auto& contentArray = outputBlock["content"];
                const auto& content0     = outputBlock["content"][0];
                if (contentArray.size() > 1){
                    // the API spec says only one item is inside at a time.
                    APATE_LOG_DEBUG("Unexpected: content array has more than one object inside");
                }

                if(content0.contains("refusal")){
                    message.message = content0["refusal"];
                    message.refused = true;
                }
                else if (content0.contains("text")){
                    message.message = content0["text"];
                }
                else{
                    APATE_LOG_WARN_AND_THROW(std::runtime_error, "Message content missing 'refusal' or 'text'");
                }
            }
            else {
                APATE_LOG_WARN_AND_THROW(std::runtime_error, "Json content block is invalid");
            }

            item.content = message;
            break;
        }
        case OutputItemType::OUTPUT_REASONING:
        {
            openai::chatGPTOutputReasoning reasoning;
            reasoning.id = outputBlock.value("id", "");

            if(outputBlock.contains("summary") && outputBlock["summary"].array() && !outputBlock["summary"].empty()){
                if (outputBlock["summary"].contains("text")){
                    reasoning.summary = outputBlock["summary"]["text"];
                }
            }
            item.content = reasoning;
            break;
        }
        default:
        {
            APATE_LOG_WARN_AND_THROW(std::runtime_error, "unimplemented response type: {}", std::string (outputBlock["type"]));
            break;
        }
    }
    return item;
}

nlohmann::json chatGPTPrompt::JsonRequest(void) const{
    nlohmann::json json;
    json["model"] = model.modelValue;
    json["instructions"] = systemPrompt;

    // oldest first.
    for (size_t ii = 0; ii < history.size(); ii++){

        const auto &msg = history[ii];
        json["input"][ii]["role"] = (ROLE_USER == msg.role) ? "user" : "assistant";
        json["input"][ii]["content"] = msg.message;
    }

    size_t lastMessage = json["input"].size();
    json["input"][lastMessage]["role"]    = "user";
    json["input"][lastMessage]["content"] = request;

    return json;
}


void chatGPTResponse::Put(const nlohmann::json& json){
    if(json.empty()){
        APATE_LOG_WARN_AND_THROW(std::runtime_error, "Empty json object");
    }

    if(!json.contains("id")){
        APATE_LOG_DEBUG("Id missing from json object");
    }
    else{
        id = json["id"];
    }

    if(!json.contains("status")){
        APATE_LOG_DEBUG("status missing from json object");
    }
    else{
        status = json["status"];
    }

    if(json.contains("created_at")){
        createdAt = json["created_at"];
    }

    if(json.contains("error")){
        std::string errorCode;
        std::string errorReason;

        if(json["error"].contains("code")){
            errorCode = json["error"]["code"];
        }

        if(json["error"].contains("reason")){
            errorReason = json["error"]["reason"];
        }

        if (!errorCode.empty() || !errorReason.empty()){
            responseFailureReason = std::format("ERROR CODE {} - {}",
                                                errorCode.empty() ? "NONE" : errorCode,
                                                errorReason);
        }
    }

    responseOK = (status == "completed" && responseFailureReason.empty());

    if(!json.count("output")){
        APATE_LOG_WARN("json object has no output objects");
        return;
    }

    auto &jsonOutputs = json["output"];
    for (const auto& outputBlock : jsonOutputs) {
        try{
            chatGPTOutputItem item = OutputItemFromJson(outputBlock);
            outputs.push_back(std::move(item));
        }
        catch (const std::exception &e){
            APATE_LOG_WARN ("Output block could not be parsed. - '{}'. Dump: \n\n{}",
                            e.what(),
                            jsonOutputs.dump());
        }
        catch (...){
            APATE_LOG_WARN ("Output block could not be parsed. Unknown exception. Dump: \n\n{}",
                            jsonOutputs.dump());
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
    chatGPTDispatchRequest toDispatch;
    toDispatch.json    = prompt.JsonRequest();

    auto future        = toDispatch.promise.get_future();

    std::lock_guard lock(m_dispatchQMtx);

    m_dispatchQ.push(std::move(toDispatch));
    m_dispatchQCV.notify_all();

    return future;
}
void chatGPT::HandleQueue(void){
    while(true){
        std::unique_lock lock(m_dispatchQMtx);
        m_dispatchQCV.wait(lock, [&](){return (m_shutDown || !m_dispatchQ.empty());});

        if (m_shutDown){
            // clear out the queue and send a status code any threads waiting on a promise

            while(!m_dispatchQ.empty()){
                chatGPTDispatchRequest request (std::move (m_dispatchQ.front()));
                chatGPTResponse        response;

                response.HTTPCode = CURLE_RECV_ERROR;
                request.promise.set_value(response);

                m_dispatchQ.pop();
            }
            return;
        }

        // copy the data to avoid starving callers while we process
        chatGPTDispatchRequest request (std::move (m_dispatchQ.front()));
        m_dispatchQ.pop();
        lock.unlock();

        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("Authorization: Bearer " + m_openAI_Key).c_str());

        std::string requestDataStr = request.json.dump();

        APATE_LOG_DEBUG(requestDataStr);


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
            try{
                nlohmann::json jsonResponse = nlohmann::json::parse(curlResponse);
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
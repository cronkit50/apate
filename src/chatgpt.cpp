#include "chatgpt.hpp"

#include <nlohmann/json.hpp>
namespace openai
{
chatGPT::chatGPT(void)
{
    CURL* curl = NULL;
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
}

void chatGPT::SetKey(const std::string_view& string){
    m_openAI_Key = string;
}
}
#include "chatgpt.hpp"

#include <nlohmann/json.hpp>
namespace openai
{
chatGPT::chatGPT(void){

}

void chatGPT::SetKey(const std::string_view& string){
    m_openAI_Key = string;
}
}
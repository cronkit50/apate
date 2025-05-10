#ifndef OPENAI_HPP
#define OPENAI_HPP

#include <curl/curl.h>

#include <string>
namespace openai
{
class chatGPT{
public:
    chatGPT(void);


    void SetKey(const std::string_view& string);


private:
    std::string m_openAI_Key;
};
}
#endif
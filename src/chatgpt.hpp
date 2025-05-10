#ifndef OPENAI_HPP
#define OPENAI_HPP

#include <curl/curl.h>

#include <future>
#include <string>
#include <thread>

namespace openai
{
class chatGPT{
public:
    chatGPT(void);

    void SetKey(const std::string_view& string);

private:
    std::string m_openAI_Key;

    // to do, use a thread pool?
    std::thread m_dispatcher;
};
}
#endif
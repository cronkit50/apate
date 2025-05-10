#include "discordbot.hpp"


#include <string>
#include <string_view>
#include <mutex>

namespace discord
{
discordBot::discordBot(const std::string& discordAPIToken)
    : m_api (discordAPIToken),
      m_cluster(discordAPIToken, dpp::i_default_intents | dpp::i_message_content)
{
    auto botHandle = [this]() {
        this->m_cluster.start(dpp::st_wait);

        // wake up anyone waiting
        m_botThreadWaitFlag = false;
        m_botThreadWaitCV.notify_all();
        };

    m_botThread = std::thread(botHandle);
}
discordBot::~discordBot(){
    m_cluster.shutdown();
    m_botThread.join();
}

void discordBot::Wait(){
    std::unique_lock lock(m_botThreadWaitMtx);
    m_botThreadWaitCV.wait(lock, [&]()->bool{return (m_botThreadWaitFlag);});
}
}
#include "discordbot.hpp"

namespace discord
{
discordBot::discordBot(const std::string& discordAPIToken)
    : m_api (discordAPIToken),
      m_cluster(discordAPIToken, dpp::i_default_intents | dpp::i_message_content)
{
    auto botHandle = [this]() {
        try
        {
            this->m_cluster.start(dpp::st_return);
            m_botStartedOK = true;
        }
        catch (...)
        {
            m_botStartedOK = false;
        }

        // wake up anyone waiting
        m_botThreadWaitFlag = true;
        m_botThreadWaitCV.notify_all();
        };

    m_botThread = std::thread(botHandle);
}
discordBot::~discordBot(){
    m_cluster.shutdown();

    if (m_botThread.joinable()){
        m_botThread.join();
    }
}

bool discordBot::WaitForStart(){
    std::unique_lock lock(m_botThreadWaitMtx);
    m_botThreadWaitCV.wait(lock, [&]()->bool{return (m_botThreadWaitFlag);});
    return m_botStartedOK;
}
}
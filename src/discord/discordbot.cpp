#include "discordbot.hpp"

namespace discord
{
discordBot::discordBot(const std::string& discordAPIToken)
    : m_api (discordAPIToken),
      m_cluster(discordAPIToken, dpp::i_default_intents | dpp::i_message_content)
{
    auto messageCreateHandler = std::bind(&discordBot::HandleMessageEvent, this, std::placeholders::_1);
    auto readyHandler         = std::bind(&discordBot::HandleOnReady,      this, std::placeholders::_1);

    m_cluster.on_message_create(messageCreateHandler);
    m_cluster.on_ready(readyHandler);
}


void discordBot::Start(void){
    std::lock_guard lock (m_botThreadWaitMtx);

    m_botStartedOK      = false;
    m_botThreadWaitFlag = false;
    // to-do, handle bot-restarts.

    try {
        // let dpp call our on_ready callback to set the return flag
        this->m_cluster.start(dpp::st_return);
    }
    catch (...){
        // set the return flag ourselves and wake up any waiters
        m_botStartedOK      = false;
        m_botThreadWaitFlag = true;
        m_botThreadWaitCV.notify_all();
    }
}


discordBot::~discordBot(){
    m_cluster.shutdown();
}


bool discordBot::WaitForStart(){
    std::unique_lock lock(m_botThreadWaitMtx);
    m_botThreadWaitCV.wait(lock, [&]()->bool{return (m_botThreadWaitFlag);});
    return m_botStartedOK;
}

void discordBot::SetPersistence(discord::serverPersistence&& persistence){
    m_persistence = std::forward<discord::serverPersistence>(persistence);
}


void discordBot::HandleOnReady(const dpp::ready_t& event){
    m_botStartedOK      = true;
    m_botThreadWaitFlag = true;
    m_botThreadWaitCV.notify_all();
}


void discordBot::HandleMessageEvent(const dpp::message_create_t& event){
    std::lock_guard lock(m_eventCallbackMtx);
    m_persistence.RecordMessageEvent(event);
}

}
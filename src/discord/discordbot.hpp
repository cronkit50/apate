#ifndef DISCORDBOT_HPP
#define DISCORDBOT_HPP

#include <discord/serverpersistence.hpp>

#include <dpp/cluster.h>

#include <chatgpt.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace discord
{
class discordBot
{
public:
    discordBot(const std::string& discordAPIToken);
    discordBot(const discordBot&)            = delete;
    discordBot& operator=(const discordBot&) = delete;
    discordBot(discordBot&&)                 = delete;
    discordBot& operator=(discordBot&&)      = delete;



    void Start(void);

    ~discordBot();
    bool WaitForStart();
    void SetPersistence(discord::serverPersistence &&persistence);

    void SetChatGPT(openai::chatGPT *chatGPT);

private:
    void HandleOnSlashCommand(const dpp::slashcommand_t& event);
    void HandleOnReady(const dpp::ready_t& event);
    void HandleMessageEvent(const dpp::message_create_t &event);

    std::string  m_api;
    dpp::cluster m_cluster;

    std::atomic<bool>       m_botStartedOK      = false;
    std::atomic<bool>       m_botThreadWaitFlag = false;
    std::mutex              m_botThreadWaitMtx;
    std::condition_variable m_botThreadWaitCV;

    std::mutex              m_eventCallbackMtx;

    serverPersistence       m_persistence;

    openai::chatGPT*        m_chatGPT = nullptr;
};
}

#endif
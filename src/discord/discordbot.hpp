#ifndef DISCORDBOT_HPP
#define DISCORDBOT_HPP

#include "discord/messagearchiver.hpp"

#include <dpp/cluster.h>

#include <chatgpt.hpp>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#define DEFAULT_AI_MODEL      "chatgpt-4o-latest"
#define DEFAULT_AI_MODEL_FAST "gpt-4o-mini"

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
    void SetWorkingDir(const std::filesystem::path dir);
    void SetChatGPT(std::unique_ptr<openai::chatGPT> &&chatGPT);

private:
    void HandleOnSlashCommand(const dpp::slashcommand_t& event);
    void HandleOnReady(const dpp::ready_t& event);
    void HandleMessageEvent(const dpp::message_create_t &event);

    void StartArchiving(const dpp::ready_t& event);

    std::string  m_model = DEFAULT_AI_MODEL;


    std::string  m_api;
    dpp::cluster m_cluster;

    std::atomic<bool>       m_botStartedOK      = false;
    std::atomic<bool>       m_botThreadWaitFlag = false;
    std::mutex              m_botThreadWaitMtx;
    std::condition_variable m_botThreadWaitCV;

    std::unique_ptr<openai::chatGPT> m_chatGPT;

    std::filesystem::path m_workingDir;

    messageArchiver m_messageArchiver;

    size_t m_OnStartFetchAmount = 5;
    size_t m_ContinousFetchAmount = 50;
    size_t m_chatGPTMessageContextRequirement = 50;
    size_t m_chatGPTPrefilterContextRequirement = 50;
    size_t m_chatGPTLongTermContextRequirement = 500;
};
}

#endif
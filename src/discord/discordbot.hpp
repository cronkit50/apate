#ifndef DISCORDBOT_HPP
#define DISCORDBOT_HPP

#include <dpp/cluster.h>

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
    ~discordBot();
    void Wait();

private:
    std::string  m_api;
    dpp::cluster m_cluster;

    std::thread  m_botThread;

    std::atomic<bool>       m_botThreadWaitFlag = false;
    std::mutex              m_botThreadWaitMtx;
    std::condition_variable m_botThreadWaitCV;
};
}

#endif
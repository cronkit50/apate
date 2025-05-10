#ifndef DISCORDBOT_HPP
#define DISCORDBOT_HPP

#include <dpp/cluster.h>

namespace discord
{
class ChatGptAgent
{
public:
    ChatGptAgent(const std::string& discordAPIToken);

private:
    std::string  m_api;
    dpp::cluster m_cluster;
};
}

#endif
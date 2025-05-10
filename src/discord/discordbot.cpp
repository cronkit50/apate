#include "discordbot.hpp"


#include <string>
#include <string_view>

namespace discord
{
ChatGptAgent::ChatGptAgent(const std::string& discordAPIToken)
    : m_api (discordAPIToken),
      m_cluster(discordAPIToken, dpp::i_default_intents | dpp::i_message_content)
{



}
}
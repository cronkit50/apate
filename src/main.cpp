#include "cfg/cfgFile.hpp"
#include "common/common.hpp"
#include "common/util.hpp"
#include "chatgpt.hpp"
#include "discord/discordbot.hpp"
#include "log/log.hpp"

#include <iostream>
#include <filesystem>

int main(int argc, char* argv[]) {

    curl_global_init (CURL_GLOBAL_DEFAULT);

    AddOnLog([](const logMessage& _, const std::string& formatted){ std::cout << formatted << '\n';});

    APATE_LOG_INFO("Starting Apate...");

    CfgFile cfg;
    cfg.ReadCfgFile(GetDirectory(DIRECTORY_CFG, "ENV.cfg").string());

    openai::chatGPT* chatGPT = new openai::chatGPT(cfg.ReadPpty<std::string>("OPEN_API_KEY"));
    discord::serverPersistence persistence;
    persistence.SetBaseDirectory(GetDirectory(DIRECTORY_PERSISTENCE));

    discord::discordBot discordBot(cfg.ReadPpty<std::string>("DISCORD_BOT_KEY"));
    discordBot.SetPersistence(std::move(persistence));
    discordBot.SetChatGPT(chatGPT);
    discordBot.Start();
    discordBot.WaitForStart();

    APATE_LOG_INFO("Discord Bot started...");

    std::promise<void>().get_future().wait();
}

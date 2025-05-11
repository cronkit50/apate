#include "cfg/cfgFile.hpp"
#include "common/common.hpp"
#include "common/util.hpp"
#include "chatgpt.hpp"
#include "discord/discordbot.hpp"

#include <iostream>
#include <filesystem>

int main(int argc, char* argv[]) {

    curl_global_init (CURL_GLOBAL_DEFAULT);


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

    std::promise<void>().get_future().wait();
}

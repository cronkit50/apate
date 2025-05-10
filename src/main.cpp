#include "cfg/cfgFile.hpp"
#include "common/common.hpp"
#include "common/util.hpp"
#include "chatgpt.hpp"
#include "discord/discordbot.hpp"

#include <iostream>
#include <filesystem>

int main(int argc, char* argv[]) {
    CfgFile cfg;
    cfg.ReadCfgFile(GetDirectory(DIRECTORY_CFG, "ENV.cfg").string());

    discord::serverPersistence persistence;
    persistence.SetBaseDirectory(GetDirectory(DIRECTORY_PERSISTENCE));

    discord::discordBot discordBot(cfg.ReadPpty<std::string>("DISCORD_BOT_KEY"));
    discordBot.SetPersistence(std::move(persistence));
    discordBot.Start();
    discordBot.WaitForStart();

    std::promise<void>().get_future().wait();
}

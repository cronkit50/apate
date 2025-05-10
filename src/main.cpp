#include "cfg/cfgFile.hpp"
#include "common/common.hpp"
#include "chatgpt.hpp"
#include "discord/discordbot.hpp"

#include <iostream>
#include <filesystem>

int main(int argc, char* argv[]) {

    char exePath[MAX_BUFF_SIZE] = "";
    GetModuleFileNameA(NULL, exePath, sizeof(exePath));

    CfgFile cfg;
    std::string cfgPath = exePath;
    cfgPath = cfgPath.substr(0, cfgPath.find_last_of('\\')) + "\\..\\ENV.cfg";
    cfg.ReadCfgFile(cfgPath);

    discord::ChatGptAgent(cfg.ReadPpty<std::string>("DISCORD_BOT_KEY"));

    std::cout << "Hello World!";
}

// DPP extraneous warnings

#include "cfg/cfgFile.hpp"
#include "common/common.hpp"

#include <iostream>
#include <filesystem>
#include <dpp/cluster.h>
int main(int argc, char* argv[]) {
    
    char exePath[MAX_BUFF_SIZE] = "";
    GetModuleFileNameA(NULL, exePath, sizeof(exePath));

    CfgFile cfg;
    std::string cfgPath = exePath;
    cfgPath = cfgPath.substr(0, cfgPath.find_last_of('\\')) + "\\..\\ENV.cfg";
    cfg.ReadCfg(cfgPath);


    

    std::cout << "Hello World!";
}

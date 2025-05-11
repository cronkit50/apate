#ifndef CFG_HPP
#define CFG_HPP

#include "cfg/cfgFile.hpp"

#include <filesystem>
#include <memory>

enum cfg_file : int{
    CFG_FILE_ENV
};

template <>
struct std::formatter<cfg_file> : std::formatter<std::string> {
    auto format(cfg_file c, std::format_context& ctx) const {
        std::string str;
        switch (c) {
            case CFG_FILE_ENV:
                str = "ENV";
                break;
            default:
                str = "UNKNOWN";
        }
        return std::formatter<std::string>::format(str, ctx);
    }
};

std::shared_ptr<CfgFile> CfgGetFile(const cfg_file fileType);
std::shared_ptr<CfgFile> CfgGetFile(const std::filesystem::path &pathToCfg);


#endif
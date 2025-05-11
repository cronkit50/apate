#ifndef CFG_HPP
#define CFG_HPP

#include "cfg/cfgFile.hpp"

#include <filesystem>
#include <memory>

std::shared_ptr<CfgFile> CfgGetFile(const std::filesystem::path &pathToCfg);


#endif
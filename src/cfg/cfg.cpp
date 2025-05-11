#include "cfg.hpp"

#include "common/lrucache.hpp"
#include "common/util.hpp"

#include "log/log.hpp"

#include <format>
#include <map>
#include <mutex>
#include <string>

std::shared_ptr<CfgFile> CfgGetFile(const cfg_file fileType){
    std::shared_ptr<CfgFile> cfgFile;
    switch (fileType){
        case CFG_FILE_ENV:
        {
            auto pathToCfg = GetDirectory(DIRECTORY_CFG, "ENV.cfg");
            cfgFile = CfgGetFile(pathToCfg);
            break;
        }
        default:
        {
            APATE_LOG_DOMAIN_AND_THROW(std::runtime_error, "FileType: {} unsupported", fileType);
            break;
        }
    }

    return cfgFile;
}

std::shared_ptr<CfgFile> CfgGetFile(const std::filesystem::path& pathToCfg){
    static std::mutex cfgCacheMutex;
    static lruCache<std::string, std::shared_ptr<CfgFile>> cfgCache;

    std::string              fullPath (ToLowercase(std::filesystem::absolute (pathToCfg).string()));
    std::shared_ptr<CfgFile> cfgFile;

    std::lock_guard lock(cfgCacheMutex);

    if(!cfgCache.Get(fullPath, cfgFile)){

        std::shared_ptr<CfgFile> newCfgFile = std::make_shared<CfgFile>();
        try{
            newCfgFile->ReadCfgFile(fullPath);
            std::swap (cfgFile, newCfgFile);
        } catch(const std::exception &e){
            APATE_LOG_WARN_AND_THROW (std::runtime_error, "Config file '{}' could not be read - {}", fullPath, e.what());
        } catch (...){
            throw;
        }
    }
    return cfgFile;
}

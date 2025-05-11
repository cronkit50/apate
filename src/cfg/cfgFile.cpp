#include "cfgFile.hpp"

#include "log/log.hpp"
#include "common/common.hpp"
#include "common/util.hpp"

#include <cstdlib>
#include <filesystem>
#include <exception>
#include <fstream>
#include <sstream>
#include <functional>
#include <optional>
#include <regex>
#include <format>

#include <windows.h>
#include <processenv.h>

#define CFG_FILE_LINE_DELIM          "\n"
#define CFG_FILE_KEY_VALUE_SEPARATOR "="


static std::string ConfigPostProcessing(const std::string_view str){
    // Windows environment variables can be letters, numbers and underscores
    static const std::regex envVariableRgx("%[\\w_]+%");

    std::string processed(str);
    std::string possibleEnvVariable (StripSpaces(str));

    if (std::regex_match(possibleEnvVariable, envVariableRgx)){
        // environment variable - strip the delimiters
        possibleEnvVariable.resize(possibleEnvVariable.size() - 1);
        possibleEnvVariable = possibleEnvVariable.substr(1);

        char buffer[MAX_BUFF_SIZE] = "";
        GetEnvironmentVariableA(possibleEnvVariable.c_str (), buffer, sizeof (buffer));
        processed = buffer;
    }

    return processed;

}


static bool ShouldIgnoreCfgLine(const std::string_view& str){
    std::string_view stripped = StripSpaces(str);

    bool ignoreLine = false;

    if (stripped.empty()) {
        // whitespace line
        ignoreLine = true;
    }
    else if (stripped.size() >= 2 && stripped.substr(0,2) == "//") {
        // comment
        ignoreLine = true;
    }

    return ignoreLine;
}

static std::pair<CfgKey, CfgVal> ParseCfgSingleLine(const std::string_view& cfgLine){
    // Raw regex: ^\s*(.+?)\s*=\s*(.+)\s*$
    // C++-ified: ^\\s*(.+?)\\s*=\\s*(.+)\\s*$

    // Accept anything so long as its equal seperated.
    static const std::regex rgx("^\\s*(.+?)\\s*=\\s*(.+)\\s*$");

    std::string stripped (StripSpaces(cfgLine));
    std::smatch matches;

    if (!std::regex_search(stripped, matches, rgx)){
        APATE_LOG_WARN_AND_THROW(std::runtime_error, "Config val '{}' has invalid format", cfgLine);
    }

    return std::make_pair(matches[1], ConfigPostProcessing(matches[2].str ()));
}

void CfgFile::ReadCfgFile(const std::string_view pathToCfg){
    std::fstream inputStream;

    if (!std::filesystem::exists(pathToCfg)){
        APATE_LOG_WARN_AND_THROW(std::invalid_argument, "Cannot find config file '{}'", pathToCfg);

    }

    inputStream.open(pathToCfg, std::ios_base::in);
    if (!inputStream.is_open()){
        APATE_LOG_WARN_AND_THROW(std::invalid_argument, "Failed to open config file '{}'", pathToCfg);
    }

    m_cfg.clear();
    m_configFilePath = pathToCfg;


    std::stringstream buffer;
    buffer << inputStream.rdbuf();

    ParseCfgLines(buffer.str());
}

std::string CfgFile::ConfigFilePath() const{
    return m_configFilePath;
}

void CfgFile::ParseCfgLines(const std::string& buffer){
    std::vector<std::string_view> lines = Tokenize(buffer, CFG_FILE_LINE_DELIM);

    for (const auto& line : lines){
        if (ShouldIgnoreCfgLine(line)){
            continue;
        }

        try{
            m_cfg.insert(ParseCfgSingleLine(line));
        }
        catch (...){
            APATE_LOG_WARN("Config line: {} failed to parse", line);
        }
    }
}


template<>
int CfgFile::ReadPpty<int>(const std::string_view key) {
    const std::string keyBuff (key);

    int parsed = 0;
    if (m_cfg.count(keyBuff) <= 0) {
        APATE_LOG_WARN_AND_THROW(std::runtime_error, "Key: {} doesn't exist", keyBuff);
    }

    try {
        parsed = std::stoi(m_cfg.at(keyBuff));
    }
    catch (const std::exception &){
        APATE_LOG_WARN_AND_THROW(std::runtime_error, "Key = {}, Value = {} failed to parse as int", key, m_cfg.at(keyBuff));
    }
    return parsed;
}

template<>
std::string CfgFile::ReadPpty<std::string>(const std::string_view key){
    const std::string keyBuff(key);

    if(m_cfg.count(keyBuff) <= 0){
        APATE_LOG_WARN_AND_THROW(std::runtime_error, "No such key {}", keyBuff);
    }

    return m_cfg.at(keyBuff);
}

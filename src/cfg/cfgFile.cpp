#include "cfgFile.hpp"

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
        std::string msg = std::vformat("Config val '{}' has invalid format",
                                        std::make_format_args(cfgLine));

        throw std::runtime_error(msg);
    }

    return std::make_pair(matches[1], ConfigPostProcessing(matches[2].str ()));
}

void CfgFile::ReadCfgFile(const std::string_view pathToCfg){
    std::fstream inputStream;

    if (!std::filesystem::exists(pathToCfg)){
        std::string msg = std::vformat("Cannot find config file '{}'",
                                        std::make_format_args(pathToCfg));

        throw std::invalid_argument(msg);
    }

    inputStream.open(pathToCfg, std::ios_base::in);
    if (!inputStream.is_open()){
        std::string msg = std::vformat("Failed to open config file '{}'",
                                        std::make_format_args(pathToCfg));

        throw std::runtime_error(msg);
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
            // keep going even if one line failed
        }
    }
}


template<>
int CfgFile::ReadPpty<int>(const std::string_view key) {
    const std::string keyBuff (key);

    int parsed = 0;
    if (m_cfg.count(keyBuff) <= 0) {
        std::string msg = std::vformat("Key: {} doesn't exist",
                                        std::make_format_args(keyBuff));

        throw std::runtime_error(msg);
    }

    try {
        parsed = std::stoi(m_cfg.at(keyBuff));
    }
    catch (const std::exception &){
        std::string msg = std::vformat("Key = {}, Value = {} failed to parse as int",
                                        std::make_format_args (key, m_cfg.at(keyBuff)));
        throw std::runtime_error(msg);
    }
    return parsed;
}

template<>
std::string CfgFile::ReadPpty<std::string>(const std::string_view key){
    const std::string keyBuff(key);

    if(m_cfg.count(keyBuff) <= 0){
        std::string msg = "No such key '" + keyBuff + "' in " + ConfigFilePath();
        throw std::runtime_error(msg);
    }

    return m_cfg.at(keyBuff);
}

#include "cfgFile.hpp"

#include "common/util.hpp"

#include <filesystem>
#include <exception>
#include <fstream>
#include <sstream>
#include <functional>
#include <optional>
#include <regex>
#include <format>

#define CFG_FILE_LINE_DELIM          "\n"
#define CFG_FILE_KEY_VALUE_SEPARATOR "="

static bool ShouldIgnoreCfgLine(const std::string_view& str)
{
    bool ignoreLine = false;

    std::string_view stripped = StripSpaces(str);

    if (stripped.empty()) {
        // whitespace line
        ignoreLine = true;
    }
    else if (stripped.size() >= 2 && stripped.substr(0,2) == "//")
    {
        // comment
        ignoreLine = true;
    }

    return ignoreLine;
    
}

static std::pair<CfgKey, CfgVal> ReadCfgLine(const std::string_view& str)
{
    // Raw regex: ^\s*(.+?)\s*=\s*(.+)\s*$
    // C++-ified: ^\\s*(.+?)\\s*=\\s*(.+)\\s*$

    // Accept anything so long as its equal seperated.
    static const std::regex rgx("^\\s*(.+?)\\s*=\\s*(.+)\\s*$");

    std::string stripped (StripSpaces(str));
    std::smatch matches;

    if (!std::regex_search(stripped, matches, rgx))
    {
        std::string msg = std::vformat("Config val '{}' has invalid format",
                                        std::make_format_args(str));
    }

    return std::make_pair(matches[1], matches[2]);
}

void CfgFile::ReadCfg(const std::string_view pathToCfg)
{
    std::fstream inputStream;

    if (!std::filesystem::exists(pathToCfg))
    {
        std::string msg = std::vformat("Cannot find config file '{}' has invalid format",
                                        std::make_format_args(pathToCfg));

        throw std::invalid_argument(msg);
    }

    inputStream.open(pathToCfg, std::ios_base::in);
    if (!inputStream.is_open())
    {
        std::string msg = std::vformat("Failed to open config file '{}'",
                                        std::make_format_args(pathToCfg));

        throw std::runtime_error(msg);
    }

    m_configFilePath = pathToCfg;


    std::stringstream buffer;
    buffer << inputStream.rdbuf();

    ParseCfgLines(buffer.str());
}

std::string CfgFile::ConfigFilePath() const
{
    return m_configFilePath;
}

void CfgFile::ParseCfgLines(const std::string& buffer)
{
    m_cfg.clear();

    std::vector<std::string_view> lines = Tokenize(buffer, CFG_FILE_LINE_DELIM);

    for (const auto& line : lines)
    {
        if (ShouldIgnoreCfgLine(line))
        {
            continue;
        }

        try
        {
            m_cfg.emplace(ReadCfgLine(line));
        }
        catch (...)
        {
            // stop parsing and wipe the config cache clean.
            m_cfg.clear();

            throw;
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
std::string CfgFile::ReadPpty<std::string>(const std::string_view key)
{
    static const std::regex rgx("^[\"](.*?)[\"]$");

    const std::string keyBuff(key);


    if (m_cfg.count(keyBuff) <= 0)
    {
        std::string msg = "No such key '" + keyBuff + "' in " + ConfigFilePath();

        throw std::runtime_error(msg);
    }
    std::string val = m_cfg.at(keyBuff);
    std::smatch valMatcher;

    if(!std::regex_search(val, valMatcher, rgx))
    {
        std::string msg = std::vformat("Key = {}, Value = {} failed to parse as string",
                                       std::make_format_args(key, m_cfg.at(keyBuff)));

        throw std::runtime_error(msg);
    }

    return valMatcher[1];
}

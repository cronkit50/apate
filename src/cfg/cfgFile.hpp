#ifndef CFGFILE_HPP
#define CFGFILE_HPP

#include <string>
#include <string_view>
#include <map>
 
typedef std::string CfgKey;
typedef std::string CfgVal;

class CfgFile {
public:
    void ReadCfg(const std::string_view pathToCfg);
    std::string ConfigFilePath() const;

    template<typename T>
    T ReadPpty(const std::string_view key);

    // only support these for now.
    template<> int         ReadPpty<int>        (const std::string_view key);
    template<> std::string ReadPpty<std::string>(const std::string_view key);

    ~CfgFile() = default;
private:

    void ParseCfgLines(const std::string &buffer);


    std::map<CfgKey, CfgVal> m_cfg;
    std::string m_configFilePath;
};

#endif
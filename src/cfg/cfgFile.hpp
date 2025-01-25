#ifndef CFGFILE_HPP
#define CFGFILE_HPP

#include <sstream>
#include <string>
#include <map>
#include <variant>

typedef std::variant<std::string, int> CfgValueVariant;

struct CfgItem {
    std::string key;
    CfgValueVariant value;
};

class CfgFile {
public:
    CfgFile() = default;

    void Read(const std::string_view& pathToCfg);

    ~CfgFile() = default;
private:

    void Parse(std::stringstream& buffer);

    std::map<std::string, CfgItem> m_cfg;
};

#endif
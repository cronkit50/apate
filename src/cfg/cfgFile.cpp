#include "cfgFile.hpp"

#include "util.hpp"

#include <filesystem>
#include <exception>
#include <fstream>
#include <sstream>


void CfgFile::Read(const std::string_view& pathToCfg)
{
    std::fstream inputStream;

    if (!std::filesystem::exists(pathToCfg))
    {
        std::string msg = "Config file: \"" + std::string(pathToCfg) + "\" doesn't exist";
        throw std::invalid_argument(msg);
    }

    inputStream.open(pathToCfg, std::ios_base::in);
    if (!inputStream.is_open())
    {
        std::string msg = "Config file: \"" + std::string(pathToCfg) + "\" failed to open";
        throw std::runtime_error(msg);
    }


    std::stringstream buffer;
    buffer << inputStream.rdbuf();


    Parse(buffer);



}

void CfgFile::Parse(std::stringstream &buffer)
{
    std::vector<std::string> lines = SplitString(buffer.str(), "\n");

    for (const auto& line : lines)
    {
        size_t equalsPos = line.find_first_of('=');

        if (equalsPos == std::string::npos)
        {

        }

    }
}

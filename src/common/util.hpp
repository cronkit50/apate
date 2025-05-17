#ifndef UTIL_HPP
#define UTIL_HPP

#include <dpp/dpp.h>

#include <cstddef>
#include <filesystem>
#include <vector>
#include <string>

enum DIRECTORY{
    DIRECTORY_EXE,
    DIRECTORY_CFG,
    DIRECTORY_PERSISTENCE,
    DIRECTORY_DELIMITER
};

std::filesystem::path GetDirectory(const DIRECTORY directory, const std::string_view fileName = "");


// splits a string by some arbitrary delimiter
std::vector<std::string_view> Tokenize(const std::string_view view, const std::string_view input);

std::string_view StripSpaces(const std::string_view view);


constexpr std::size_t operator "" _zu(unsigned long long const n){
    return static_cast<std::size_t>(n);
}

unsigned long long SnowflakeToUnix(const dpp::snowflake& flake);


dpp::snowflake SnowflakeNow();
dpp::snowflake UnixToSnowflake(const unsigned long long& unixTime);

std::string SnowflakeFriendly (const dpp::snowflake &flake);

std::string ReplaceSubstring(const std::string_view& original,
                             const std::string_view& find,
                             const std::string_view& replace);

std::string ToLowercase (const std::string_view &view);

size_t GetSessionToken();

#endif
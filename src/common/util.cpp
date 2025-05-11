#include "util.hpp"

#include <common/common.hpp>

#include <atomic>
#include <format>
#include <windows.h>
#include <libloaderapi.h>
#include <map>
#include <mutex>
#include <stdexcept>
#include <time.h>

static std::atomic<bool>                          directoriesInited = false;
static std::map<DIRECTORY, std::filesystem::path> directories;

std::filesystem::path GetDirectory(const DIRECTORY directory, const std::string_view fileName){
    static std::mutex startUpMtx;

    std::unique_lock unique(startUpMtx);
    if(!directoriesInited){
        wchar_t exePath[MAX_BUFF_SIZE] = L"";
        GetModuleFileName(NULL, exePath, sizeof(exePath));

        std::filesystem::path exeDir = exePath;
        exeDir.remove_filename();

        directories[DIRECTORY_EXE]         = exeDir;
        directories[DIRECTORY_CFG]         = std::filesystem::path(exeDir).append("..\\cfg\\");
        directories[DIRECTORY_PERSISTENCE] = std::filesystem::path(exeDir).append("..\\persistence\\");
        directoriesInited = true;

    }
    unique.unlock();

    if (directory == DIRECTORY_DELIMITER){
        throw std::runtime_error("Directory type invalid");
    }

    auto path = directories.at (directory);
    return (path.append(fileName));
}

std::vector<std::string_view> Tokenize(const std::string_view view, const std::string_view delim){
    std::vector<std::string_view> split;
    if (delim.empty() || view.empty()){
        return split;
    }

    size_t lastPos = 0;
    size_t currPos = 0;
    while ((currPos = view.find(delim, lastPos)) != std::string::npos){
        split.emplace_back(view.substr(lastPos,
                           currPos - lastPos));

        lastPos = currPos + delim.size();
    }

    // add trailing tokens
    if (lastPos < view.size()){
        split.emplace_back(view.substr(lastPos,
                           view.size() - lastPos));
    }

    return split;
}

std::string_view StripSpaces(const std::string_view view){
    if (view.empty()){
        return view;
    }
    size_t leadingSpaces  = view.find_first_not_of(' ');
    size_t trailingSpaces = view.find_last_not_of(' ');

    return view.substr(leadingSpaces, trailingSpaces - leadingSpaces + 1);
}

unsigned long long SnowflakeToUnix(const dpp::snowflake& flake){
    return (flake >> 22) + 1420070400000; // discord epoch starts at 2015, convert it to 1970 epoch
}

std::string SnowflakeFriendly(const dpp::snowflake& flake){
    // seconds
    long long timeStampSinceEpoch = SnowflakeToUnix(flake) / 1000;

    std::time_t time(timeStampSinceEpoch);
    tm localTime{ 0 };

    localtime_s(&localTime, &time);

    return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}",
                       localTime.tm_year + 1900,
                       localTime.tm_mon + 1,
                       localTime.tm_mday,

                       localTime.tm_hour,
                       localTime.tm_min,
                       localTime.tm_sec);
}


std::string ReplaceSubstring(const std::string_view& original, const std::string_view& find, const std::string_view& replace)
{
    std::string replaced(original);

    size_t pos = replaced.find(find);
    // Repeat till end is reached
    while (pos != std::string::npos)
    {
        // Replace this occurrence of Sub String
        replaced.replace(pos, find.size(), replace);
        // Get the next occurrence from the current position
        pos = replaced.find(find, pos + replace.size());
    }

    return replaced;
}

std::string ToLowercase(const std::string_view& view){
    std::string lowerCase;
    lowerCase.reserve(view.length());

    std::transform(view.begin(), view.end(), std::back_inserter(lowerCase),
                   [](unsigned char c){ return std::tolower(c); });

    return lowerCase;
}

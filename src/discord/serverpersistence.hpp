#ifndef SERVER_PERSISTENCE_HPP
#define SERVER_PERSISTENCE_HPP

#include <dpp/dpp.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>

namespace discord
{
struct channelRecordFile{
    std::filesystem::path pathToFile;
    std::fstream          oStream;

};

struct messageRecord{

    messageRecord(const dpp::message_create_t& event);

    dpp::snowflake snowflake;

    std::string message;
    long long timeStampUnixMs;
    std::string timeStampFriendly;

    std::string authorGlobalName;
    std::string authorUserName;

    void Serialize(std::ostream& outStream);
};

class serverPersistence{
public:
    serverPersistence();
    ~serverPersistence() = default;

    serverPersistence(serverPersistence &rhs) = delete;
    serverPersistence(serverPersistence &&rhs) noexcept;
    serverPersistence& operator=(serverPersistence &&rhs) noexcept;

    void SetBaseDirectory(const std::filesystem::path &dir);
    void RecordMessageEvent(const dpp::message_create_t& event);

    void swap(serverPersistence& rhs);

private:
    channelRecordFile &GetChannelFile(const dpp::message_create_t& event);

    std::filesystem::path m_baseDir;

    // by channel id
    std::map<dpp::snowflake, channelRecordFile> m_channelLogs;
};
}

#endif
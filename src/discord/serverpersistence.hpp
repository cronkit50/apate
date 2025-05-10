#ifndef SERVER_PERSISTENCE_HPP
#define SERVER_PERSISTENCE_HPP

#include <dpp/dpp.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <list>

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
    ~serverPersistence();

    serverPersistence(serverPersistence &rhs) = delete;
    serverPersistence(serverPersistence &&rhs) noexcept;
    serverPersistence& operator=(serverPersistence &&rhs) noexcept;

    void SetBaseDirectory(const std::filesystem::path &dir);
    void SetLocalMessageCacheLimit(const size_t numMessages);

    void RecordMessageEvent(const dpp::message_create_t& event);

    serverPersistence& swap(serverPersistence& rhs);

private:

    void CloseOpenHandles();
    channelRecordFile &GetChannelFile(const dpp::message_create_t& event);

    std::filesystem::path m_baseDir;

    size_t m_localMessageCacheMax = 200;

    // by channel id
    std::map<dpp::snowflake, channelRecordFile> m_channelLogFiles;
    std::map<dpp::snowflake, std::list<messageRecord>> m_messagesByChannel;
};
}

#endif
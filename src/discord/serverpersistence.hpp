#ifndef SERVER_PERSISTENCE_HPP
#define SERVER_PERSISTENCE_HPP

#include <dpp/dpp.h>

#include <filesystem>
#include <fstream>
#include <list>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace discord
{
struct channelRecordFile{
    std::filesystem::path pathToFile;
    std::fstream          fStream;

};

struct messageRecord{

    messageRecord(const dpp::message_create_t& event);
    messageRecord(void) = default;
    dpp::snowflake snowflake;

    std::string message;
    long long timeStampUnixMs;
    std::string timeStampFriendly;

    std::string authorGlobalName;
    std::string authorUserName;
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
    dpp::snowflake GetLastMessageID(const dpp::snowflake& channelID) const;

    std::vector<messageRecord> GetMessagesByChannel(const dpp::snowflake& channelID, const size_t numMessages);
    serverPersistence& swap(serverPersistence& rhs);

private:

    void CloseOpenHandles();


    bool DoesHistoryExistForChannel(const dpp::snowflake& channelID);
    std::shared_ptr<channelRecordFile> GetChannelFile(const dpp::message_create_t& event, const bool makeIfNotExist = true);

    std::filesystem::path m_baseDir;

    size_t m_localMessageCacheMax = 200;

    // by channel id
    std::map<dpp::snowflake, std::shared_ptr<channelRecordFile>> m_channelLogFiles;
    std::map<dpp::snowflake, std::list<messageRecord>> m_messagesByChannel;
};
}

#endif
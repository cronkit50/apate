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
    channelRecordFile() = default;
    channelRecordFile(channelRecordFile&) = delete;
    channelRecordFile(channelRecordFile&&) = delete;
    channelRecordFile& operator=(channelRecordFile&) = delete;
    channelRecordFile& operator=(channelRecordFile&&) = delete;

    ~channelRecordFile(){
        if(fStream.is_open()){
            fStream.close();
        }
    }

};

struct messageRecord{

    messageRecord(const dpp::message& event);
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

    void RecordMessage(const dpp::message& msg);
    void RecordMessages(const dpp::message_map &messages);
    size_t GetContinuousMessages(const dpp::snowflake snowflake);
    std::vector<messageRecord> GetMessagesByChannel(const dpp::snowflake& channelID, const size_t numMessages);
    serverPersistence& swap(serverPersistence& rhs);

private:

    void CloseOpenHandles();


    bool DoesHistoryExistForChannel(const dpp::snowflake& channelID);
    std::shared_ptr<channelRecordFile> GetChannelFile(const dpp::snowflake channelId, const bool makeIfNotExist = true);

    std::filesystem::path m_baseDir;

    size_t m_localMessageCacheMax = 200;

    // by channel id
    std::map<dpp::snowflake, std::shared_ptr<channelRecordFile>> m_channelLogFiles;
    std::map<dpp::snowflake, std::list<messageRecord>> m_messagesByChannel;
};
}

#endif
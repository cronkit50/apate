#ifndef SERVER_PERSISTENCE_HPP
#define SERVER_PERSISTENCE_HPP

#include <log/log.hpp>

#include <dpp/dpp.h>
#include <sqlite3/sqlite3.h>

#include <filesystem>
#include <fstream>
#include <list>
#include <map>
#include <string>
#include <string_view>
#include <vector>


namespace discord
{

struct messageRecord{

    messageRecord(const dpp::message& event);
    messageRecord(void) = default;

    dpp::snowflake channelId;
    dpp::snowflake snowflake;

    std::string message;
    long long timeStampUnixMs;
    std::string timeStampFriendly;

    std::string authorGlobalName;
    std::string authorUserName;
};


struct persistenceDatabase{

    typedef int sql_rc;

    std::string databaseName;
    std::string databaseFile;

    sqlite3 *db = nullptr;

    persistenceDatabase(void) = default;
    persistenceDatabase(const std::filesystem::path &pathToDb);
    void Open(const std::filesystem::path &pathToDb);

    void Close();

    bool IsOpen(void) const;

    persistenceDatabase(persistenceDatabase&) = delete;
    persistenceDatabase(persistenceDatabase&&) = delete;
    persistenceDatabase& operator=(persistenceDatabase&) = delete;
    persistenceDatabase& operator=(persistenceDatabase&&) = delete;

    persistenceDatabase& operator<<(const messageRecord& message);

    sql_rc GetLatestMessagesByChannel(const dpp::snowflake channelId, const size_t numMessages, std::vector<messageRecord> &message);

    ~persistenceDatabase();

private:
    sql_rc CreateChannelMessagesTable(const dpp::snowflake channelId);
    std::string GetTableName(const dpp::snowflake channelId) const;

};


class serverPersistence{
public:
    serverPersistence();
    ~serverPersistence();

    serverPersistence(serverPersistence &rhs) = delete;
    serverPersistence(serverPersistence &&rhs) noexcept;
    serverPersistence& operator=(serverPersistence &&rhs) noexcept;

    void SetBaseDirectory(const std::filesystem::path &dir);

    void RecordMessage(const dpp::message& msg);
    void RecordMessages(const dpp::message_map &messages);
    size_t GetContinuousMessages(const dpp::snowflake snowflake);
    std::vector<messageRecord> GetMessagesByChannel(const dpp::snowflake& channelID, const size_t numMessages);
    serverPersistence& swap(serverPersistence& rhs);

private:

    bool DoesHistoryExistForChannel(const dpp::snowflake& channelID);
    std::shared_ptr<persistenceDatabase> GetDbHandle(const bool makeIfNotExist = true);

    std::filesystem::path m_baseDir;


    std::shared_ptr<persistenceDatabase> m_persistenceDatabase;
};
}

#endif
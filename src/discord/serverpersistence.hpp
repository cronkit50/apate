#ifndef SERVER_PERSISTENCE_HPP
#define SERVER_PERSISTENCE_HPP

#include <log/log.hpp>

#include <dpp/dpp.h>
#include <sqlite3.h>

#include <filesystem>
#include <fstream>
#include <list>
#include <map>
#include <string>
#include <string_view>
#include <vector>


namespace discord
{

struct embeddingRecord{
    dpp::snowflake     messageId;
    std::vector<float> embedding;
};

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
    dpp::snowflake authorId;
};


struct persistenceDatabase{

private:
    typedef std::pair<dpp::snowflake, dpp::snowflake> rangePair;

public:

    typedef int sql_rc;

    std::string databaseName;
    std::string databaseFile;



    persistenceDatabase(void) = default;
    persistenceDatabase(const std::filesystem::path &pathToDb);
    void Open(const std::filesystem::path &pathToDb);

    void Close();

    bool IsOpen(void) const;

    persistenceDatabase(persistenceDatabase&) = delete;
    persistenceDatabase(persistenceDatabase&&) = delete;
    persistenceDatabase& operator=(persistenceDatabase&) = delete;
    persistenceDatabase& operator=(persistenceDatabase&&) = delete;

    sql_rc StoreContinousMessages(const std::vector<messageRecord>& messages, const dpp::snowflake lastMessageId = {});
    sql_rc StoreContinousMessage(const messageRecord& message, const dpp::snowflake lastMessageId = {});

    sql_rc GetLatestMessagesByChannel(const dpp::snowflake channelId, const size_t numMessages, std::vector<messageRecord> &message);
    size_t GetContinuousMessages(const dpp::snowflake channelId, const dpp::snowflake since);
    dpp::snowflake GetOldestContinuousTimestamp(const dpp::snowflake channelId, const dpp::snowflake since);
    sql_rc StoreEmbedding(const dpp::snowflake channelId, const dpp::snowflake messageId, std::vector<float>& embedding);
    bool HasEmbedding(const dpp::snowflake channelId, const dpp::snowflake messageId);

    bool FindMessage(const dpp::snowflake& channelID, const dpp::snowflake messageId, messageRecord& message);

    std::vector<embeddingRecord> GetVectorEmbeddings(const dpp::snowflake channelId);

    ~persistenceDatabase();

private:


    persistenceDatabase& operator<<(const messageRecord& message);
    persistenceDatabase& operator<<(const std::vector<messageRecord> &messages);


    sqlite3 *m_sqlite3_db = nullptr;

    rangePair ComputeMessageRange (const std::vector<messageRecord>& messages, const dpp::snowflake lastMessageId = {});
    std::vector<rangePair> FetchOverlappingRanges(const std::string tableName, const rangePair &range);
    void DeleteContinuityEntry(const std::string tableName, const dpp::snowflake entry);
    void CreateContinuityEntry(const std::string tableName, const rangePair &range);

    sql_rc CreateChannelTables(const dpp::snowflake channelId);
    std::string GetMessagesTableName(const dpp::snowflake channelId) const;
    std::string GetContinuityTrackTableName(const dpp::snowflake channelId) const;
    std::string GetEmbeddingsTableName(const dpp::snowflake channelId) const;
};


class serverPersistence{
public:
    serverPersistence();
    ~serverPersistence();

    serverPersistence(serverPersistence &rhs) = delete;
    serverPersistence(serverPersistence &&rhs) noexcept;
    serverPersistence& operator=(serverPersistence &&rhs) noexcept;

    void SetBaseDirectory(const std::filesystem::path &dir);

    void RecordLatestMessage(const dpp::message& msg);
    void RecordLatestMessages(const dpp::message_map &messages);
    void RecordLatestMessages(const dpp::snowflake channelId, const std::vector<messageRecord> &messages);

    void RecordOldMessagesContinuous(const dpp::snowflake channelId, const std::vector<messageRecord> &messages);
    void RecordOldMessagesContinuous(const dpp::message_map &messages);

    size_t CountContinuousMessages(const dpp::snowflake channelId, const dpp::snowflake since);

    void SaveEmbedding (const dpp::snowflake channelId, const dpp::snowflake messageId, std::vector<float>& embedding);
    bool HasEmbedding(const dpp::snowflake channelId, const dpp::snowflake messageId);

    dpp::snowflake GetOldestContinuousTimestamp(const dpp::snowflake channelId, const dpp::snowflake since);

    std::vector<messageRecord> GetContinousMessagesByChannel(const dpp::snowflake& channelID, const size_t numMessages);
    bool FindMessage(const dpp::snowflake& channelID, const dpp::snowflake messageId, messageRecord& message);

    std::vector<embeddingRecord> GetVectorEmbeddings(const dpp::snowflake channelId);

    serverPersistence& swap(serverPersistence& rhs);

private:

    bool DoesHistoryExistForChannel(const dpp::snowflake& channelID);
    std::shared_ptr<persistenceDatabase> GetDbHandle(const bool makeIfNotExist = true);

    std::filesystem::path m_baseDir;

    std::map<dpp::snowflake, dpp::snowflake> m_latestMessageByChannel;

    std::shared_ptr<persistenceDatabase> m_persistenceDatabase;
};
}

#endif
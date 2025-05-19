#ifndef MESSAGEARCHIVER_HPP
#define MESSAGEARCHIVER_HPP

#include <discord/serverpersistence.hpp>


#include <dpp/dpp.h>

#include <filesystem>
#include <vector>

namespace discord{
class messageArchiver{
private:
    struct serverPersistenceWrapper{
        serverPersistenceWrapper() = default;
        serverPersistenceWrapper(serverPersistenceWrapper&) = delete;
        serverPersistenceWrapper(serverPersistence&& rhs){
            persistence = std::move(rhs);
        }
        serverPersistenceWrapper& operator=(serverPersistenceWrapper&) = delete;
        serverPersistenceWrapper& operator=(serverPersistenceWrapper&& rhs){
            persistence = std::move(rhs.persistence);
            return *this;
        }
        serverPersistence persistence;
        std::mutex        mutex;
    };

public:
    messageArchiver(void);
    messageArchiver(const std::filesystem::path &persistenceDir);
    messageArchiver(messageArchiver&) = delete;
    messageArchiver(messageArchiver&&) = delete;
    messageArchiver& operator=(messageArchiver&) = delete;
    messageArchiver& operator=(messageArchiver&&) = delete;
    ~messageArchiver(void) = default;

    void SetPersistenceDir(const std::filesystem::path& dir);
    void RecordLatestMessage(const dpp::message& message);
    void BatchRecordLatestMessages(const dpp::snowflake guildId,const dpp::snowflake channelId, const dpp::message_map& messages);

    size_t CountContinousMessages(const dpp::snowflake guildId, const dpp::snowflake channelId, const dpp::snowflake since);

    dpp::snowflake GetOldestContinuousTimestamp(const dpp::snowflake guildId, const dpp::snowflake channelId, const dpp::snowflake since);

    std::vector<messageRecord> GetContinousMessages(const dpp::snowflake guildId,
                                                    const dpp::snowflake channelId,
                                                    const size_t         numMessages);

private:

    serverPersistenceWrapper& GetGuildPersistence(const dpp::snowflake& guildID);


    std::mutex                                         m_persistenceDictMtx;
    std::map<dpp::snowflake, serverPersistenceWrapper> m_persistenceByGuild;
    std::filesystem::path                              m_persistenceDir;


};
}

#endif
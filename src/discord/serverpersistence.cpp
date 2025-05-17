#include "serverpersistence.hpp"

#include "log/log.hpp"
#include "common/util.hpp"

#include <filesystem>
#include <sstream>


#define SERVER_PERSISTENCE_DB_FILENAME "persistence.db"

static std::filesystem::path BuildPathToDatabase(const std::filesystem::path& baseDir){
    std::filesystem::path pathToChannel(baseDir.string() + SERVER_PERSISTENCE_DB_FILENAME);

    return pathToChannel;
}

namespace discord{

persistenceDatabase::persistenceDatabase(const std::filesystem::path& pathToDb){
    Open(pathToDb);
}

void persistenceDatabase::Open(const std::filesystem::path& pathToDb){

    Close();

    if(!std::filesystem::exists(pathToDb)){
        APATE_LOG_INFO("Creating new database {}", pathToDb.string())

        std::filesystem::path dbDir = pathToDb;
        std::filesystem::create_directories(dbDir.remove_filename());

        int rc = SQLITE_OK;

        if(rc = sqlite3_open(pathToDb.string ().c_str (), &db) != SQLITE_OK){
            APATE_LOG_WARN_AND_THROW(std::runtime_error,
                                     "Failed to open sqlite3 database {} - {}",
                                     pathToDb.string(),
                                     sqlite3_errstr(rc));
        }
    }

    databaseFile = pathToDb.string();
    databaseName = pathToDb.filename().string();
}

void persistenceDatabase::Close(){
    int rc = SQLITE_OK;

    if(!db){
        return;
    }

    if((rc = sqlite3_close(db)) != SQLITE_OK){
        APATE_LOG_WARN_AND_THROW(std::runtime_error,
                                 "sqlite3_close({}) failed - {}",
                                 databaseFile,
                                 sqlite3_errstr(rc));
    }
    else{

        APATE_LOG_INFO("closed sqlite3 database {}",
                       databaseFile);

        db = nullptr;

        databaseFile.clear();
        databaseName.clear();
    }
}

bool persistenceDatabase::IsOpen(void) const{
    return db;
}

persistenceDatabase& persistenceDatabase::operator<<(const messageRecord& message){
    if(!IsOpen()){
        APATE_LOG_WARN("sqlite3 database {} is not open",
                       databaseFile);
        return *this;
    }

    char* sqlError = nullptr;
    int   rc       = SQLITE_OK;

    std::string tableName = GetTableName(message.channelId);

    std::string sql = std::format("INSERT INTO {} (snowflake, authorUserName, authorGlobalName, timeStampUnixMs, timeStampFriendly, message) "
                                  "VALUES ({}, '{}', '{}', {}, '{}', '{}');",
                                  tableName,
                                  message.snowflake.str(),
                                  message.authorUserName,
                                  message.authorGlobalName,
                                  message.timeStampUnixMs,
                                  message.timeStampFriendly,
                                  message.message);

    if(rc = CreateChannelMessagesTable(message.channelId) != SQLITE_OK){
        APATE_LOG_WARN("{} - Failed to create table for channel {} - {}",
                       databaseFile,
                       tableName,
                       sqlite3_errstr(rc));
    }
    else if((rc = sqlite3_exec(db, sql.c_str(), NULL, NULL, &sqlError)) != SQLITE_OK){
        APATE_LOG_WARN("{} Failed to insert message into sqlite3 database {} - {}",
                       databaseFile,
                       tableName,
                       sqlite3_errstr(rc));

        sqlite3_free(sqlError);
    }

    return *this;
}

discord::persistenceDatabase::sql_rc persistenceDatabase::GetLatestMessagesByChannel(const dpp::snowflake channelId, const size_t numMessages, std::vector<messageRecord> &message){


    std::vector<messageRecord> messages;

    if(!numMessages){
        message = messages;
        return SQLITE_OK;
    }
    if(!IsOpen()){
        APATE_LOG_WARN("sqlite3 database {} is not open",
                       databaseFile);
        return SQLITE_INTERNAL;
    }

    sql_rc rc = SQLITE_OK;

    // descending order, latest first
    std::string sql = std::format("SELECT * FROM {} ORDER BY snowflake DESC LIMIT {};",
                                  GetTableName(channelId),
                                  numMessages);

    if(rc = CreateChannelMessagesTable(channelId) != SQLITE_OK){
        APATE_LOG_WARN("{} - Failed to create table for channel {} - {}",
                       databaseFile,
                       channelId.str(),
                       sqlite3_errstr(rc));
    }
    else if (rc = (sqlite3_exec(db,
                                sql.c_str(),
                                [](void* data, int argc, char** argv, char** azColName){
                                    std::vector<messageRecord>* messages = static_cast<std::vector<messageRecord>*>(data);

                                    messageRecord msg;
                                    msg.snowflake = dpp::snowflake(std::stoull(argv[0]));
                                    msg.authorUserName = argv[1];
                                    msg.authorGlobalName = argv[2];
                                    msg.timeStampUnixMs = std::stoll(argv[3]);
                                    msg.timeStampFriendly = argv[4];
                                    msg.message = argv[5];
                                    messages->push_back(msg);
                                    return 0;
                                },
                                &messages, nullptr) != SQLITE_OK)){


        APATE_LOG_WARN("{} - Failed to get messages from sqlite3 database {} - {}",
                        databaseFile,
                        sqlite3_errstr(rc));
        }

    if (SQLITE_OK == rc){
        message = messages;
    }

    return rc;

}

persistenceDatabase::~persistenceDatabase(){
    try{
        Close();
    } catch(...){
        APATE_LOG_WARN("{} - Failed to close sqlite3 database",
                       databaseFile);
    }
}

discord::persistenceDatabase::sql_rc persistenceDatabase::CreateChannelMessagesTable(const dpp::snowflake channelId){
    if(channelId.empty()){
        APATE_LOG_DEBUG("Channel ID is empty");
        return SQLITE_ERROR;
    }

    sql_rc rc     = SQLITE_OK;
    char*  errMsg = nullptr;

    const std::string sql = std::format ("CREATE TABLE IF NOT EXISTS {} ("
                                         "snowflake INTEGER PRIMARY KEY,"
                                         "authorUserName TEXT NOT NULL,"
                                         "authorGlobalName TEXT NOT NULL,"
                                         "timeStampUnixMs INTEGER NOT NULL,"
                                         "timeStampFriendly TEXT NOT NULL,"
                                         "message TEXT);",
                                         GetTableName(channelId));

    if(rc = sqlite3_exec(db, sql.c_str(), NULL, NULL, &errMsg) != SQLITE_OK){

        APATE_LOG_WARN("sqlite3_exec({}) failed - {}",
                       sql,
                       sqlite3_errstr(rc));

        sqlite3_free(errMsg);
    }

    return rc;
}

std::string persistenceDatabase::GetTableName(const dpp::snowflake channelId) const {

    std::string tableName = "messages_" + channelId.str ();
    return tableName;
}


messageRecord::messageRecord(const dpp::message& msg){
    channelId = msg.channel_id;
    snowflake = msg.id;
    message = msg.content;
    timeStampUnixMs = SnowflakeToUnix(snowflake);
    timeStampFriendly = SnowflakeFriendly(snowflake);
    authorGlobalName = msg.author.global_name;
    authorUserName = msg.author.username;
}


serverPersistence::serverPersistence(){
    wchar_t pathBuff[MAX_PATH] = L"";
    GetModuleFileName(NULL, pathBuff, sizeof(pathBuff));

    // default to the executable directory
    SetBaseDirectory(GetDirectory(DIRECTORY_EXE).string());
}

serverPersistence::~serverPersistence(){

}

serverPersistence::serverPersistence(serverPersistence&& rhs) noexcept{
    m_baseDir = std::move(rhs.m_baseDir);
    m_persistenceDatabase = std::move(rhs.m_persistenceDatabase);
}

serverPersistence& serverPersistence::operator=(serverPersistence&& rhs) noexcept{
    if(&rhs!=this){
        serverPersistence temp(std::forward<serverPersistence>(rhs));
        swap(temp);

    }
    return *this;
}

void serverPersistence::SetBaseDirectory(const std::filesystem::path& dir){
    m_baseDir = dir;
    m_baseDir.remove_filename();
}


void serverPersistence::RecordMessage(const dpp::message& msg){
    messageRecord messageRecord(msg);

    std::shared_ptr<persistenceDatabase> dbHandle;
    if((dbHandle = GetDbHandle(msg.channel_id)) == nullptr){
        APATE_LOG_WARN_AND_THROW(std::runtime_error,
                                 "Failed to open file for channel {}",
                                 msg.channel_id.str());
    }
    else{
        *dbHandle<<messageRecord;
    }
}

void serverPersistence::RecordMessages(const dpp::message_map& messages){
    for(const auto& [_, message]:messages){
        try{

            RecordMessage(message);

        } catch(const std::exception& e){
            APATE_LOG_WARN("Failed to record message {} - {}",
                           message.id.str (),
                           e.what());
        }

    }
}

size_t serverPersistence::GetContinuousMessages(const dpp::snowflake snowflake){
    size_t num = 0;

    std::shared_ptr<persistenceDatabase> channelFile = GetDbHandle(snowflake);

    return 0;
}


std::vector<messageRecord> serverPersistence::GetMessagesByChannel(const dpp::snowflake& channelID, const size_t numMessages){

    std::shared_ptr<persistenceDatabase> channelFile = GetDbHandle();
    std::vector<messageRecord> messages;

    if(nullptr==channelFile){
        APATE_LOG_WARN("Failed to get database handle for channel {}", channelID.str());
    }
    else{
        persistenceDatabase::sql_rc rc = channelFile->GetLatestMessagesByChannel(channelID, numMessages, messages);

        if(rc != SQLITE_OK){
            APATE_LOG_WARN("Failed to get messages from channel {} - {}",
                           channelID.str(),
                           sqlite3_errstr(rc));
        }
    }

    return messages;
}

serverPersistence& serverPersistence::swap(serverPersistence& rhs){
    if(&rhs!=this){
        std::swap(m_baseDir, rhs.m_baseDir);
        std::swap(m_persistenceDatabase, rhs.m_persistenceDatabase);

    }

    return *this;
}


bool serverPersistence::DoesHistoryExistForChannel(const dpp::snowflake& channelID){
    return !GetMessagesByChannel(channelID, 1).empty();
}

std::shared_ptr<persistenceDatabase> serverPersistence::GetDbHandle(const bool makeIfNotExist){

    bool retrieved = false;

    if(!m_persistenceDatabase && makeIfNotExist){

        const std::filesystem::path logFilePath = BuildPathToDatabase(m_baseDir);

        try{
            m_persistenceDatabase = std::make_shared<persistenceDatabase>(logFilePath);

        } catch(const std::exception& e){
            APATE_LOG_WARN("Failed to create channel log file {} - {}",
                           logFilePath.string(),
                           e.what());
        }

    }
    return m_persistenceDatabase;
}
}

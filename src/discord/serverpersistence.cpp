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
    }

    int rc = SQLITE_OK;

    if((rc = sqlite3_open(pathToDb.string ().c_str (), &m_sqlite3_db)) != SQLITE_OK){
        APATE_LOG_WARN_AND_THROW(std::runtime_error,
                                 "Failed to open sqlite3 database {} - {}",
                                 pathToDb.string(),
                                 sqlite3_errstr(rc));
    }

    databaseFile = pathToDb.string();
    databaseName = pathToDb.filename().string();
}

void persistenceDatabase::Close(){
    int rc = SQLITE_OK;

    if(!m_sqlite3_db){
        return;
    }

    if((rc = sqlite3_close(m_sqlite3_db)) != SQLITE_OK){
        APATE_LOG_WARN_AND_THROW(std::runtime_error,
                                 "sqlite3_close({}) failed - {}",
                                 databaseFile,
                                 sqlite3_errmsg(m_sqlite3_db));
    }
    else{

        APATE_LOG_INFO("closed sqlite3 database {}",
                       databaseFile);

        m_sqlite3_db = nullptr;

        databaseFile.clear();
        databaseName.clear();
    }
}

bool persistenceDatabase::IsOpen(void) const{
    return m_sqlite3_db;
}

persistenceDatabase& persistenceDatabase::operator<<(const messageRecord& message){
    if(!IsOpen()){
        APATE_LOG_WARN("sqlite3 database {} is not open",
                       databaseFile);
        return *this;
    }

    char* sqlError = nullptr;
    int   rc       = SQLITE_OK;

    std::string tableName = GetMessagesTableName(message.channelId);

    sqlite3_stmt* stmt = nullptr;

    std::string sql = std::format("INSERT OR IGNORE INTO {} (snowflake, channelsnowflake, authorUserName, authorGlobalName, authorId, timeStampUnixMs, timeStampFriendly, message) "
                                  "VALUES ({}, {}, ?, ?, {}, {}, '{}', ?);",
                                  tableName,

                                  message.snowflake.str(),
                                  message.channelId.str(),
                                  message.authorId.str(),
                                  message.timeStampUnixMs,
                                  message.timeStampFriendly);


    if((rc = CreateChannelTables(message.channelId)) != SQLITE_OK){
        APATE_LOG_WARN("{} - Failed to create table for channel {} - {}",
                       databaseFile,
                       tableName,
                       sqlite3_errstr(rc));
    }
    else if ((rc = sqlite3_prepare_v2(m_sqlite3_db, sql.c_str (), -1, &stmt, NULL) != SQLITE_OK)){
        APATE_LOG_WARN("{} - Failed to prepare statement for message {} - {}",
                       databaseFile,
                       message.snowflake.str(),
                       sqlite3_errmsg(m_sqlite3_db));
    }
    else if ((rc = sqlite3_bind_text(stmt, 1, message.authorUserName.c_str (), -1, SQLITE_TRANSIENT) != SQLITE_OK)){
        APATE_LOG_WARN("{} - Failed to bind message {} - {}",
                       databaseFile,
                       message.authorUserName,
                       sqlite3_errmsg(m_sqlite3_db));
    }
    else if ((rc = sqlite3_bind_text(stmt, 2, message.authorGlobalName.c_str (), -1, SQLITE_TRANSIENT) != SQLITE_OK)){
        APATE_LOG_WARN("{} - Failed to bind message {} - {}",
                       databaseFile,
                       message.authorGlobalName,
                       sqlite3_errmsg(m_sqlite3_db));
    }
    else if ((rc = sqlite3_bind_text(stmt, 3, message.message.c_str (), -1, SQLITE_TRANSIENT) != SQLITE_OK)){
        APATE_LOG_WARN("{} - Failed to bind message {} - {}",
                       databaseFile,
                       message.message,
                       sqlite3_errmsg(m_sqlite3_db));
    }
    else{
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            // process each row
        }
        if(sqlite3_errcode(m_sqlite3_db)!=SQLITE_DONE){
            APATE_LOG_WARN("{} - Failed to insert message {} into {} - {}",
                           databaseFile,
                           message.snowflake.str(),
                           tableName,
                           sqlite3_errmsg(m_sqlite3_db));
        }
    }

    if(sqlite3_finalize(stmt) != SQLITE_OK){
        APATE_LOG_WARN("{} - Failed to finalize statement for message {} - {}",
                       databaseFile,
                       message.snowflake.str(),
                       sqlite3_errmsg(m_sqlite3_db));
    }

    return *this;
}

persistenceDatabase& persistenceDatabase::operator<<(const std::vector<messageRecord>& messages){
    for (const auto &message : messages){
        *this<<message;
    }

    return *this;
}



discord::persistenceDatabase::rangePair persistenceDatabase::ComputeMessageRange(const std::vector<messageRecord>& messages, const dpp::snowflake adjacentMessageId){

    dpp::snowflake earliestMessage = adjacentMessageId;
    dpp::snowflake latestMessage = adjacentMessageId;

    if(messages.empty()){
        return std::make_pair(earliestMessage, latestMessage);
    }

    // fallback if we arent provided a last message.
    if (adjacentMessageId.empty ()){
        earliestMessage = messages.front().snowflake;
        latestMessage   = messages.back().snowflake;
    }


    for(const auto& message:messages){
        earliestMessage = std::min(earliestMessage, message.snowflake);
        latestMessage   = std::max(latestMessage, message.snowflake);
    }

    return std::make_pair(earliestMessage, latestMessage);
}

std::vector<discord::persistenceDatabase::rangePair> persistenceDatabase::FetchOverlappingRanges(const std::string tableName, const rangePair &range){

    std::vector<discord::persistenceDatabase::rangePair> matchingRanges;

    if(!IsOpen()){
        APATE_LOG_WARN("sqlite3 database {} is not open",
                       databaseFile);
        return matchingRanges;
    }

    const char *findRangeSQLStmt = "SELECT * FROM ?"
                                            " WHERE (? >= snowflakeBegin AND ? < snowflakeEnd)"
                                            " OR ? >= (snowflakeBegin AND ? < snowflakeEnd);";

    std::string findRangeSQL = std::format ("SELECT * FROM {}"
                                            " WHERE ({} >= snowflakeBegin AND {} < snowflakeEnd)"
                                            " OR {} >= (snowflakeBegin AND {} < snowflakeEnd);",
                                            tableName,
                                            range.first.str (),
                                            range.first.str (),
                                            range.second.str (),
                                            range.second.str ());




    if((sqlite3_exec(m_sqlite3_db,
       findRangeSQL.c_str(),
       [](void* data, int argc, char** argv, char** azColName){
           std::vector<rangePair>* ranges = static_cast<std::vector<rangePair>*>(data);

           try{
               dpp::snowflake begin = dpp::snowflake(std::stoull(argv[0]));
               dpp::snowflake end   = dpp::snowflake(std::stoull(argv[1]));

               ranges->push_back(std::make_pair(begin, end));

           } catch(const std::exception& e){
               APATE_LOG_WARN("Failed to parse message from sqlite3 database - {}",
                              e.what());
           } catch(...){
               APATE_LOG_WARN("Failed to parse message from sqlite3 database - unknown exception");
           }

           return 0;
       }, &matchingRanges, NULL) != SQLITE_OK)){
        APATE_LOG_WARN("{} - Failed to get continuity ranges from {} - {}",
                       databaseFile,
                       tableName,
                       sqlite3_errmsg(m_sqlite3_db));
    }

    return matchingRanges;
}

void persistenceDatabase::DeleteContinuityEntry(const std::string tableName, const dpp::snowflake entry){

    std::string deleteOldRangeSQL = std::format("DELETE FROM {} WHERE {} = snowflakeBegin",
                                                tableName,
                                                entry.str ());

    if(sqlite3_exec(m_sqlite3_db, deleteOldRangeSQL.c_str(), NULL, NULL, NULL) != SQLITE_OK){
        APATE_LOG_WARN("{} - Failed to delete old range {} from {} - {}",
                       databaseFile,
                       entry.str(),
                       tableName,
                       sqlite3_errmsg(m_sqlite3_db));
    }

}

void persistenceDatabase::CreateContinuityEntry(const std::string tableName, const rangePair& range){
    std::string insertRangeSQL = std::format("INSERT INTO {} (snowflakeBegin, snowflakeEnd) VALUES ({}, {});",
                                             tableName,
                                             range.first.str(),
                                             range.second.str());

    if(sqlite3_exec(m_sqlite3_db, insertRangeSQL.c_str(), NULL, NULL, NULL) != SQLITE_OK){
        APATE_LOG_WARN("{} - Failed to insert range {} - {} into {} - {}",
                       databaseFile,
                       range.first.str(),
                       range.second.str(),
                       tableName,
                       sqlite3_errmsg(m_sqlite3_db));
    }
}


discord::persistenceDatabase::sql_rc persistenceDatabase::StoreContinousMessages(const std::vector<messageRecord>& messages, const dpp::snowflake adjacentMessageId){
    if (messages.empty ()){
        return SQLITE_OK;
    }
    if(!IsOpen()){
        APATE_LOG_WARN("sqlite3 database {} is not open",
                       databaseFile);
        return SQLITE_INTERNAL;
    }

    *this << messages;

    // handle continuity tracking
    std::string continuityTableName          = GetContinuityTrackTableName(messages[0].channelId);
    auto inputMessageRange                   = ComputeMessageRange(messages, adjacentMessageId);
    std::vector<rangePair> overlappingRanges = FetchOverlappingRanges (continuityTableName, inputMessageRange);

    // delete the old ranges
    for (const auto &range : overlappingRanges){
        DeleteContinuityEntry(continuityTableName, range.first);
    }

    // compute the superset
    dpp::snowflake newEarliest = inputMessageRange.first;
    dpp::snowflake newLatest   = inputMessageRange.second;

    for (const auto& range : overlappingRanges) {
        newEarliest = std::min(newEarliest, range.first);
        newLatest   = std::max(newLatest, range.second);
    }

    // insert the superset
    CreateContinuityEntry(continuityTableName, std::make_pair(newEarliest, newLatest));

    // to do
    return SQLITE_OK;
}

discord::persistenceDatabase::sql_rc persistenceDatabase::StoreContinousMessage(const messageRecord& message, const dpp::snowflake lastMessageId){
    return StoreContinousMessages({ message }, lastMessageId);
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
                                  GetMessagesTableName(channelId),
                                  numMessages);

    if((rc = CreateChannelTables(channelId)) != SQLITE_OK){
        APATE_LOG_WARN("{} - Failed to create table for channel {} - {}",
                       databaseFile,
                       channelId.str(),
                       sqlite3_errstr(rc));
    }
    else if ((rc = sqlite3_exec(m_sqlite3_db,
        sql.c_str(),
        [](void* data, int argc, char** argv, char** azColName){
            std::vector<messageRecord>* messages = static_cast<std::vector<messageRecord>*>(data);

            APATE_LOG_DEBUG ("FETCHED!");
            try {
                messageRecord msg;
                msg.snowflake         = dpp::snowflake(std::stoull(argv[0]));
                msg.channelId         = dpp::snowflake(std::stoull(argv[1]));
                msg.authorUserName    = argv[2];
                msg.authorGlobalName  = argv[3];
                msg.authorId          = dpp::snowflake(std::stoull(argv[4]));
                msg.timeStampUnixMs   = std::stoll(argv[5]);
                msg.timeStampFriendly = argv[6];
                msg.message           = argv[7];

                messages->push_back(msg);


            } catch(const std::exception& e){
                APATE_LOG_WARN("Failed to parse message from sqlite3 database - {}",
                                e.what());
            } catch(...){
                APATE_LOG_WARN("Failed to parse message from sqlite3 database - unknown exception");
            }

            return 0;
        }, &messages, nullptr)) != SQLITE_OK){
        APATE_LOG_WARN("{} - Failed to get messages from sqlite3 database {} - {}",
                        databaseFile,
                       sqlite3_errmsg(m_sqlite3_db));
    }

    if (SQLITE_OK == rc){
        message = messages;
    }

    return rc;
}


size_t persistenceDatabase::GetContinuousMessages(const dpp::snowflake channelId, const dpp::snowflake since){
    size_t num = 0;

    if(!IsOpen()){
        APATE_LOG_WARN("sqlite3 database {} is not open",
                       databaseFile);
        return num;
    }

    int rc = SQLITE_OK;
    if(rc = CreateChannelTables(channelId) != SQLITE_OK){
        APATE_LOG_WARN("{} - Failed to create table for channel {} - {}",
                       databaseFile,
                       channelId.str(),
                       sqlite3_errstr(rc));

        return num;
    }

    std::string continuityTableName = GetContinuityTrackTableName(channelId);
    std::string sql = std::format("SELECT * FROM {} WHERE snowflakeBegin <= {} AND snowflakeEnd >= {}",
                                  continuityTableName,
                                  since.str(),
                                  since.str());

    rangePair range;

    if(sqlite3_exec(m_sqlite3_db, sql.c_str(), [](void* data, int argc, char** argv, char** azColName){

        try{
            dpp::snowflake begin = dpp::snowflake(std::stoull(argv[0]));
            dpp::snowflake end   = dpp::snowflake(std::stoull(argv[1]));

            auto rangePtr = static_cast<rangePair*>(data);
            rangePtr->first = begin;
            rangePtr->second = end;

        } catch(const std::exception& e){
            APATE_LOG_WARN("Failed to parse message from sqlite3 database - {}",
                           e.what());
        } catch(...){
            APATE_LOG_WARN("Failed to parse message from sqlite3 database - unknown exception");
        }
        return 0;
       }, &range, nullptr) != SQLITE_OK){
        APATE_LOG_WARN("{} - Failed to get continuity ranges from {} - {}",
                       databaseFile,
                       continuityTableName,
                       sqlite3_errmsg(m_sqlite3_db));
    }
    else{
        // we have a range of continuous snowflakes. Now actually count how many messages fall within this range.
        std::string messageTableName = GetMessagesTableName(channelId);
        std::string findMsgsSql = std::format("SELECT COUNT(*) FROM {} WHERE snowflake >= {} AND snowflake <= {}",
                                              messageTableName,
                                              range.first.str(),
                                              range.second.str());


        if(sqlite3_exec(m_sqlite3_db, findMsgsSql.c_str(), [](void* data, int argc, char** argv, char** azColName){
            size_t* num = static_cast<size_t*>(data);
            try{
                *num = std::stoull(argv[0]);
            } catch(const std::exception& e){
                APATE_LOG_WARN("Failed to parse message from sqlite3 database - {}",
                               e.what());
            } catch(...){
                APATE_LOG_WARN("Failed to parse message from sqlite3 database - unknown exception");
            }
            return 0;
           }, &num, nullptr)!=SQLITE_OK){
            APATE_LOG_WARN("{} - Failed to get messages from sqlite3 database {} - {}",
                           databaseFile,
                           messageTableName,
                           sqlite3_errmsg(m_sqlite3_db));
        }
    }
    return num;


}
dpp::snowflake persistenceDatabase::GetOldestContinuousTimestamp(const dpp::snowflake channelId, const dpp::snowflake since){
    dpp::snowflake snowflake = since;

    if(!IsOpen()){
        APATE_LOG_WARN("sqlite3 database {} is not open",
                       databaseFile);
    }
    std::string tableName = GetContinuityTrackTableName(channelId);

    std::string sql = std::format("SELECT snowflakeBegin FROM {} WHERE snowflakeBegin <= {} AND snowflakeEnd >= {}",
                                  tableName,
                                  since.str(),
                                  since.str());

    if(sqlite3_exec(m_sqlite3_db, sql.c_str(), [](void* data, int argc, char** argv, char** azColName){
        dpp::snowflake* snowflake = static_cast<dpp::snowflake*>(data);
        try{
            *snowflake = dpp::snowflake(std::stoull(argv[0]));

        } catch(const std::exception& e){
            APATE_LOG_WARN("{} - Failed to parse message from sqlite3 database - {}",
                           e.what());
        } catch(...){
            APATE_LOG_WARN("Failed to parse message from sqlite3 database - unknown exception");
        }
        return 0;
       }, &snowflake, nullptr) != SQLITE_OK){
        APATE_LOG_WARN("{} - Failed to get continuity ranges from {} - {}",
                       databaseFile,
                       tableName,
                       sqlite3_errmsg(m_sqlite3_db));
    }

    return snowflake;
}
persistenceDatabase::sql_rc persistenceDatabase::StoreEmbedding(const dpp::snowflake channelId, const dpp::snowflake messageId, std::vector<float>& embedding){
    if(embedding.empty()){
        return SQLITE_OK;
    }

    if(!IsOpen()){
        APATE_LOG_WARN("sqlite3 database {} is not open",
                       databaseFile);
        return SQLITE_INTERNAL;
    }

    std::string tableName = GetEmbeddingsTableName(channelId);
    CreateChannelTables(channelId);
    std::string sql = std::format("INSERT OR IGNORE INTO {} (snowflake, embedding) VALUES ({}, ?);",
                                  tableName,
                                  messageId.str());

    sql_rc rc = SQLITE_OK;

    sqlite3_stmt* stmt = nullptr;
    if((rc = sqlite3_prepare_v2(m_sqlite3_db, sql.c_str(), -1, &stmt, NULL)) != SQLITE_OK){
        APATE_LOG_WARN("{} - Failed to prepare statement for message {} - {}",
                       databaseFile,
                       messageId.str(),
                       sqlite3_errmsg(m_sqlite3_db));
    }
    // save the embeddings vector as memory continguous blob
    else if((rc = sqlite3_bind_blob(stmt, 1, embedding.data(), (int)embedding.size()*sizeof(float), SQLITE_TRANSIENT)) != SQLITE_OK){
        APATE_LOG_WARN("{} - sqlite3_bind_blob() failed {} - {}",
                       databaseFile,
                       messageId.str(),
                       sqlite3_errmsg(m_sqlite3_db));
    }
    else{
        while(sqlite3_step(stmt) == SQLITE_ROW){
            // process each row
        }
        if(sqlite3_errcode(m_sqlite3_db) != SQLITE_DONE){
            APATE_LOG_WARN("{} - Failed to insert message {} into {} - {}",
                           databaseFile,
                           messageId.str(),
                           tableName,
                           sqlite3_errmsg(m_sqlite3_db));
        }

        if(sqlite3_finalize(stmt)!=SQLITE_OK){
            APATE_LOG_WARN("{} - Failed to finalize statement for message {} - {}",
                           databaseFile,
                           sqlite3_errmsg(m_sqlite3_db));
        }
    }

    return rc;
}

bool persistenceDatabase::HasEmbedding(const dpp::snowflake channelId, const dpp::snowflake messageId){
    if(!IsOpen()){
        APATE_LOG_WARN("sqlite3 database {} is not open",
                       databaseFile);
        return false;
    }

    std::string tableName = GetEmbeddingsTableName(channelId);
    CreateChannelTables(channelId);
    std::string sql = std::format("SELECT COUNT(*) FROM {} WHERE snowflake = {}",
                                  tableName,
                                  messageId.str());
    size_t num = 0;
    if(sqlite3_exec(m_sqlite3_db, sql.c_str(), [](void* data, int argc, char** argv, char** azColName){
        size_t* num = static_cast<size_t*>(data);

        return 0;
       }, &num, nullptr) != SQLITE_OK){
        APATE_LOG_WARN("{} - Failed to get messages from sqlite3 database {} - {}",
                       databaseFile,
                       tableName,
                       sqlite3_errmsg(m_sqlite3_db));
    }
    return (num > 0);

}
persistenceDatabase::~persistenceDatabase(){
    try{
        Close();
    } catch(...){
        APATE_LOG_WARN("{} - Failed to close sqlite3 database",
                       databaseFile);
    }
}

persistenceDatabase::sql_rc persistenceDatabase::CreateChannelTables(const dpp::snowflake channelId){
    if(channelId.empty()){
        APATE_LOG_DEBUG("Channel ID is empty");
        return SQLITE_ERROR;
    }

    sql_rc rc     = SQLITE_OK;
    char*  errMsg = nullptr;

    const std::string createMsgTableSQL = std::format ("CREATE TABLE IF NOT EXISTS {} ("
                                                      "snowflake INTEGER PRIMARY KEY,"
                                                      "channelsnowflake INTEGER NOT NULL,"
                                                      "authorUserName TEXT NOT NULL,"
                                                      "authorGlobalName TEXT NOT NULL,"
                                                      "authorId INTEGER NOT NULL,"
                                                      "timeStampUnixMs INTEGER NOT NULL,"
                                                      "timeStampFriendly TEXT NOT NULL,"
                                                      "message TEXT);",
                                                      GetMessagesTableName(channelId));


    const std::string createContinuityTableSQL = std::format ("CREATE TABLE IF NOT EXISTS {} ("
                                                               "snowflakeBegin INTEGER PRIMARY KEY,"
                                                               "snowflakeEnd INTEGER NOT NULL)",
                                                                GetContinuityTrackTableName(channelId));


    const std::string createEmbeddingsTableSQL = std::format ("CREATE TABLE IF NOT EXISTS {} ("
                                                              "snowflake INTEGER PRIMARY KEY,"
                                                              "embedding BLOB)",
                                                              GetEmbeddingsTableName(channelId));

    if(rc = sqlite3_exec(m_sqlite3_db, createMsgTableSQL.c_str(), NULL, NULL, &errMsg) != SQLITE_OK){

        APATE_LOG_WARN("sqlite3_exec({}) failed - {}",
                       createMsgTableSQL,
                       sqlite3_errmsg(m_sqlite3_db));

        sqlite3_free(errMsg);
    }
    else if(rc = sqlite3_exec(m_sqlite3_db, createContinuityTableSQL.c_str(), NULL, NULL, &errMsg) != SQLITE_OK){

        APATE_LOG_WARN("sqlite3_exec({}) failed - {}",
                       createContinuityTableSQL,
                       sqlite3_errmsg(m_sqlite3_db));

        sqlite3_free(errMsg);
    }
    else if(rc = sqlite3_exec(m_sqlite3_db, createEmbeddingsTableSQL.c_str(), NULL, NULL, &errMsg) != SQLITE_OK){
        APATE_LOG_WARN("sqlite3_exec({}) failed - {}",
                       createEmbeddingsTableSQL,
                       sqlite3_errmsg(m_sqlite3_db));
        sqlite3_free(errMsg);
    }

    return rc;
}

std::string persistenceDatabase::GetMessagesTableName(const dpp::snowflake channelId) const {

    std::string tableName = "messages_" + channelId.str ();
    return tableName;
}

std::string persistenceDatabase::GetContinuityTrackTableName(const dpp::snowflake channelId) const{
    std::string tableName = "continuity_" + channelId.str ();
    return tableName;
}

std::string persistenceDatabase::GetEmbeddingsTableName(const dpp::snowflake channelId) const{
    std::string embeddingName = "embeddings_" + channelId.str ();
    return embeddingName;
}


messageRecord::messageRecord(const dpp::message& msg){
    channelId = msg.channel_id;
    snowflake = msg.id;
    message = msg.content;
    timeStampUnixMs = SnowflakeToUnix(snowflake);
    timeStampFriendly = SnowflakeFriendly(snowflake);
    authorGlobalName = msg.author.global_name;
    authorUserName = msg.author.username;
    authorId = msg.author.id;
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


void serverPersistence::RecordLatestMessage(const dpp::message& msg){
    RecordLatestMessages(msg.channel_id, { msg });
}

void serverPersistence::RecordLatestMessages(const dpp::message_map& messages){
    if(messages.empty()){
        return;
    }

    std::vector<messageRecord> messageRecords;

    for(const auto& [_, msg] : messages){
        messageRecords.emplace_back(msg);
    }

    RecordLatestMessages(messages.begin()->second.channel_id, messageRecords);
}

void serverPersistence::RecordLatestMessages(const dpp::snowflake channelId, const std::vector<messageRecord>& messages){
    if(messages.empty ()){
        return;
    }

    std::shared_ptr<persistenceDatabase> dbHandle = GetDbHandle(channelId);
    if(!dbHandle){
        APATE_LOG_WARN_AND_THROW(std::runtime_error,
                                 "Failed to open file for channel {}",
                                 channelId.str());
    }

    dpp::snowflake latestMessage;

    for(const auto& msg : messages){
        latestMessage = std::max(latestMessage, msg.snowflake);
    }


    if(m_latestMessageByChannel.count(channelId)){
        latestMessage = std::max (m_latestMessageByChannel[channelId], latestMessage);
        dbHandle->StoreContinousMessages(messages, latestMessage);
    }
    else{
        // weve never inserted a message, so it has no linkage
        dbHandle->StoreContinousMessages(messages);
    }

    m_latestMessageByChannel[channelId] = latestMessage;
}

void serverPersistence::RecordOldMessagesContinuous(const dpp::snowflake channelId,
                                                    const std::vector<messageRecord>& messages){
    if(messages.empty()){
        return;
    }

    std::shared_ptr<persistenceDatabase> dbHandle;
    if((dbHandle = GetDbHandle(channelId)) == nullptr){
        APATE_LOG_WARN_AND_THROW(std::runtime_error,
                                 "Failed to open file for channel {}",
                                 channelId.str());
    }
    else{
        dbHandle->StoreContinousMessages(messages);
    }
}

void serverPersistence::RecordOldMessagesContinuous(const dpp::message_map& messages){
    if(messages.empty()){
        return;
    }

    std::vector<messageRecord> messageRecords;
    for(const auto& [_, msg]:messages){
        messageRecords.emplace_back(msg);
    }
    RecordOldMessagesContinuous(messages.begin()->second.channel_id, messageRecords);
}


size_t serverPersistence::CountContinuousMessages(const dpp::snowflake channelId, const dpp::snowflake since){
    size_t num = 0;

    std::shared_ptr<persistenceDatabase> channelFile = GetDbHandle(channelId);
    if(nullptr == channelFile){
        APATE_LOG_WARN_AND_THROW(std::runtime_error,
                                 "Failed to open file for channel {}",
                                 channelId.str());
    }
    else{
        dpp::snowflake sinceActual = since;

        if (m_latestMessageByChannel.count(channelId)){

            // the latest message is older than since so we just want to know the # of continuous messages
            // since the latest.
            sinceActual = std::min(since, m_latestMessageByChannel.at(channelId));
        }

        num = channelFile->GetContinuousMessages(channelId, sinceActual);
    }

    return num;
}

void serverPersistence::SaveEmbedding(const dpp::snowflake channelId, const dpp::snowflake messageId, std::vector<float>& embedding){
    if (embedding.empty ()){
        return;
    }

    std::shared_ptr<persistenceDatabase> channelFile = GetDbHandle();
    if(!channelFile){
        APATE_LOG_WARN("Failed to get database handle for channel {}", channelId.str());
    }
    else{
        persistenceDatabase::sql_rc rc = channelFile->StoreEmbedding(channelId, messageId, embedding);
        if(rc!=SQLITE_OK){
            APATE_LOG_WARN("Failed to save embedding for message {} in channel {} - {}",
                           messageId.str(),
                           channelId.str(),
                           sqlite3_errstr(rc));
        }
    }
}

bool serverPersistence::HasEmbedding(const dpp::snowflake channelId, const dpp::snowflake messageId){
    std::shared_ptr<persistenceDatabase> channelFile = GetDbHandle();
    if(!channelFile){
        APATE_LOG_WARN("Failed to get database handle for channel {}", channelId.str());
        return false;
    }

    return channelFile->HasEmbedding(channelId, messageId);
}

dpp::snowflake serverPersistence::GetOldestContinuousTimestamp(const dpp::snowflake channelId, const dpp::snowflake since){
    dpp::snowflake snowflake = since;
    std::shared_ptr<persistenceDatabase> channelFile = GetDbHandle();
    if(!channelFile){
        APATE_LOG_WARN("Failed to get database handle for channel {}", channelId.str());
    }
    else{

        dpp::snowflake sinceActual = since;

        if (m_latestMessageByChannel.count(channelId)){

            // the latest message is older than since so we just want to know the # of continuous messages
            // since the latest.
            sinceActual = std::min(since, m_latestMessageByChannel.at(channelId));
        }

        snowflake = channelFile->GetOldestContinuousTimestamp(channelId, sinceActual);
    }
    return snowflake;
}


std::vector<messageRecord> serverPersistence::GetContinousMessagesByChannel(const dpp::snowflake& channelID, const size_t numMessages){

    std::shared_ptr<persistenceDatabase> channelFile = GetDbHandle();
    std::vector<messageRecord> messages;

    if(!channelFile){
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
    return !GetContinousMessagesByChannel(channelID, 1).empty();
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

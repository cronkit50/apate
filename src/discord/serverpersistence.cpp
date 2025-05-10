#include "serverpersistence.hpp"

#include "common/util.hpp"

#include <filesystem>
#include <sstream>

#define SERIAL_LVL_1          "    "
#define MESSAGE_LOG_EXTENSION ".log"

static std::string EscapeContent(const std::string_view& content)
{
    return ReplaceSubstring(content, "\n", "$$\\n");
}

std::string UnescapeSequence(const std::string_view& content)
{
    return ReplaceSubstring(content, "$$\\n", "\n");
}


namespace discord{

messageRecord::messageRecord(const dpp::message_create_t& event){
    snowflake         = event.msg.id;
    message           = event.msg.content;
    timeStampUnixMs   = SnowflakeToUnix (snowflake);
    timeStampFriendly = SnowflakeFriendly (snowflake);
    authorGlobalName  = event.msg.author.global_name;
    authorUserName    = event.msg.author.username;
}

void messageRecord::Serialize(std::ostream& outStream){
    outStream << snowflake                   << '\n'
        << SERIAL_LVL_1 << authorUserName    << '\n'
        << SERIAL_LVL_1 << authorGlobalName  << '\n'
        << SERIAL_LVL_1 << timeStampUnixMs   << '\n'
        << SERIAL_LVL_1 << timeStampFriendly << '\n'
        << SERIAL_LVL_1 << EscapeContent(message) << '\n';
}


serverPersistence::serverPersistence(){
    wchar_t pathBuff[MAX_PATH] = L"";
    GetModuleFileName(NULL, pathBuff, sizeof(pathBuff));

    // default to the executable directory
    SetBaseDirectory(GetDirectory(DIRECTORY_EXE).string());
}

serverPersistence::serverPersistence(serverPersistence&& rhs) noexcept{
    m_baseDir = std::move(rhs.m_baseDir);

    // close all open file handles automatically
    m_channelLogs.clear();
    m_channelLogs = std::move(rhs.m_channelLogs);

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
void serverPersistence::RecordMessageEvent(const dpp::message_create_t& event){
    channelRecordFile* fileOpen = nullptr;

    try{
        fileOpen = &GetChannelFile(event);
    } catch(...){
        // don't record, the file might not have been opened correctly
        throw;
    }

    messageRecord record(event);
    record.Serialize (fileOpen->oStream);
    fileOpen->oStream.flush ();
}

void serverPersistence::swap(serverPersistence& rhs){
    std::swap(m_baseDir, rhs.m_baseDir);
    std::swap(m_channelLogs, rhs.m_channelLogs);
}

channelRecordFile& serverPersistence::GetChannelFile(const dpp::message_create_t& event){

    channelRecordFile *recordFile = nullptr;

    dpp::snowflake channel_id = event.msg.channel_id;

    std::filesystem::path logDir (m_baseDir);
    logDir.append(event.msg.guild_id.str());

    if(m_channelLogs.count(channel_id)<=0){
        try{
            channelRecordFile newFile;

            std::filesystem::create_directories(logDir);
            newFile.pathToFile = logDir.append(channel_id.str() + MESSAGE_LOG_EXTENSION);

            // append to the end of the file
            newFile.oStream.exceptions(std::ios_base::failbit | std::ios_base::badbit);
            newFile.oStream.open(newFile.pathToFile,std::ios::app);

            auto [it, _] = m_channelLogs.insert(std::pair{ channel_id, std::move(newFile) });

            recordFile = &it->second;
        } catch(...){
            throw;
        }
    }
    else{
        // check if the file still exists
        recordFile = &m_channelLogs.at(channel_id);

        if(false == recordFile->oStream.is_open()){
            std::filesystem::create_directories(logDir);
            recordFile->oStream.open(recordFile->pathToFile,std::ios::app);
        }

    }
    return *recordFile;
}
}
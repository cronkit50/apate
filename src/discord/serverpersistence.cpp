#include "serverpersistence.hpp"

#include "log/log.hpp"
#include "common/util.hpp"

#include <filesystem>
#include <sstream>

#define SERIAL_LVL_1          "    "
#define MESSAGE_LOG_EXTENSION ".log"
#define FILE_BATCH_SIZE_BYTES 1000

static constexpr std::string_view CONTINUITY_MARKER ("-CONTINUITY-");

static std::string EscapeContent(const std::string_view& content)
{
    return ReplaceSubstring(content, "\n", "$$\\n");
}

static std::string UnescapeSequence(const std::string_view& content)
{
    return ReplaceSubstring(content, "$$\\n", "\n");
}

static std::filesystem::path BuildPathToChannelLog(const std::filesystem::path& baseDir, const dpp::snowflake& channelID){
    std::filesystem::path pathToChannel(baseDir);
    pathToChannel.append(channelID.str() + MESSAGE_LOG_EXTENSION);

    return pathToChannel;
}

static std::string MakeContinuityMarkerNow(void){
    return (std::string(CONTINUITY_MARKER).append (std::to_string (GetSessionToken ())));
}


static std::string GetCurrentLine(std::fstream& fstream) {
    if (!fstream.is_open()) {
        APATE_LOG_WARN("fstream unexpectedly closed");
        return "";
    }

    // Get current position
    const size_t originalPos = fstream.tellg();
    size_t       currPos     = originalPos;

    // Move back one character at a time until a newline or beginning of file
    std::string line;

    if (originalPos == 0){
        // nothing to be read
    }
    else{
        while(currPos>0){
            fstream.seekg(currPos--);

            // found line ending
            if(fstream.peek() == '\n'){

                // move past the newline
                currPos += 2;
                fstream.seekg(currPos);

                break;
            }
        }
        std::getline(fstream, line);
    }

    fstream.seekg(originalPos); // Restore original position

    return line;
}

static discord::messageRecord FindPrecedingNearestRecord(std::fstream &fstream){
    discord::messageRecord record;

    std::streampos originalPos = fstream.tellg();
    std::string line = GetCurrentLine (fstream);

    std::streampos currPos = originalPos;
    while (true){
        if (!line.empty() && std::isdigit(line[0])){
            // this is a message record
            std::stringstream ss(line);

            ss >> record.snowflake
               >> record.authorUserName
               >> record.authorGlobalName
               >> record.timeStampUnixMs
               >> record.timeStampFriendly;
            // read the rest of the line
            std::getline(ss, record.message);
            record.message = UnescapeSequence(record.message);
            break;
        }

        currPos = currPos - std::streamoff(line.length() + 1);

        if(currPos<=0){
            // reached the beginning of the file
            break;
        }

        line = GetCurrentLine (fstream);
    }

    return record;
}
static std::ostream& operator<<(std::ostream& outStream, const discord::messageRecord& record){
    outStream<<record.snowflake   <<                                 '\n'
             << SERIAL_LVL_1      << record.authorUserName        << '\n'
             << SERIAL_LVL_1      << record.authorGlobalName      << '\n'
             << SERIAL_LVL_1      << record.timeStampUnixMs       << '\n'
             << SERIAL_LVL_1      << record.timeStampFriendly     << '\n'
             << SERIAL_LVL_1      << EscapeContent(record.message);

    return outStream;
}



namespace discord{

messageRecord::messageRecord(const dpp::message& msg){
    snowflake         = msg.id;
    message           = msg.content;
    timeStampUnixMs   = SnowflakeToUnix (snowflake);
    timeStampFriendly = SnowflakeFriendly (snowflake);
    authorGlobalName  = msg.author.global_name;
    authorUserName    = msg.author.username;
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

    m_channelLogFiles = std::move(rhs.m_channelLogFiles);

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
void serverPersistence::SetLocalMessageCacheLimit(const size_t numMessages){
    m_localMessageCacheMax = numMessages;
}

void serverPersistence::RecordMessage(const dpp::message& msg){
    auto &messageCache = m_messagesByChannel[msg.channel_id];
    messageRecord messageRecord(msg);

    if (messageCache.size() >= m_localMessageCacheMax) {

        // reuse the front (oldest) entry and move it to the end.
        (*messageCache.begin()) = messageRecord;
        messageCache.splice(messageCache.end(), messageCache, messageCache.begin());

    }
    else{
        messageCache.push_back(messageRecord);
    }

    std::shared_ptr<channelRecordFile> fileOpen;
    if((fileOpen = GetChannelFile(msg.channel_id)) == nullptr){
        APATE_LOG_WARN_AND_THROW(std::runtime_error,
                                 "Failed to open file for channel {}",
                                 msg.channel_id.str ());
    }

    std::string line = GetCurrentLine(fileOpen->fStream);

    if(0 == line.compare(0, CONTINUITY_MARKER.size(), CONTINUITY_MARKER)){
        std::string sessionToken = line.substr(CONTINUITY_MARKER.size());

        if(sessionToken == std::to_string(GetSessionToken())){
            // this is the same session, so we need to remove the old continuity marker
            fileOpen->fStream.seekp(-std::streamoff(line.length()), std::ios::cur);
        }
        else{
            // this is a different session from the last message. Let's check if the last message is this one.
            // if not, then there could be gap between last session and now.

            discord::messageRecord nearest = FindPrecedingNearestRecord(fileOpen->fStream);
            if(nearest.snowflake == msg.id){
                // this is the same message, so we need to remove the old continuity marker
                fileOpen->fStream.seekp(-std::streamoff(line.length()), std::ios::cur);
            }
            else{
                // leave the continuity marker in place, since we don't have the history between now,
                // and the previous session
            }

        }
    }

    fileOpen->fStream << '\n';
    fileOpen->fStream << messageRecord;

    // write a continuity marker
    fileOpen->fStream << '\n';
    fileOpen->fStream << MakeContinuityMarkerNow();
    fileOpen->fStream.flush ();

}

void serverPersistence::RecordMessages(const dpp::message_map& messages){
    for (const auto&[_, message] : messages){
        RecordMessage(message);
    }
}

size_t serverPersistence::GetContinuousMessages(const dpp::snowflake snowflake){
    size_t num = 0;

    std::shared_ptr<channelRecordFile> channelFile = GetChannelFile(snowflake, false);

    if(!channelFile){
        APATE_LOG_WARN("Failed to get channel file for snowflake {}", snowflake.str());
        return num;
    }



}


std::vector<messageRecord> serverPersistence::GetMessagesByChannel(const dpp::snowflake& channelID, const size_t numMessages){
    std::vector<messageRecord> messages;

    auto it = m_messagesByChannel.begin();

    if (!numMessages){
        return messages;
    }
    else if((it = m_messagesByChannel.find(channelID)) == m_messagesByChannel.end()){
        return messages;
    }

    auto& messageCache = it->second;
    messages.reserve(std::min(messageCache.size(), numMessages));
    for(auto it = messageCache.begin(); it!=messageCache.end(); ++it){
        messages.push_back(*it);

        if(messages.size() >= numMessages){
            break;
        }
    }

    return messages;

}

serverPersistence& serverPersistence::swap(serverPersistence& rhs){
    std::swap(m_baseDir, rhs.m_baseDir);
    std::swap(m_channelLogFiles, rhs.m_channelLogFiles);

    return *this;
}

void serverPersistence::CloseOpenHandles(){
    for (auto &[_, logFile] : m_channelLogFiles){
        try{
            if(logFile->fStream.is_open()){
                logFile->fStream.close();
            }
        }
        catch(...){
            // don't emit exception for this
        }
    }
}

bool serverPersistence::DoesHistoryExistForChannel(const dpp::snowflake& channelID){
    return !GetMessagesByChannel (channelID, 1).empty();
}

std::shared_ptr<channelRecordFile> serverPersistence::GetChannelFile(const dpp::snowflake channelId, const bool makeIfNotExist){

    bool retrieved = false;

    std::shared_ptr<channelRecordFile> channelRecordFilePtrTemp;

    const std::filesystem::path logFilePath = BuildPathToChannelLog(m_baseDir, channelId);
    const std::filesystem::path logFileDir  = std::filesystem::path(logFilePath).remove_filename();

    if(m_channelLogFiles.count(channelId) > 0){
        channelRecordFilePtrTemp = m_channelLogFiles.at(channelId);
    }
    else if(!makeIfNotExist){
        // don't make a new directory
        retrieved = false;
        return nullptr;
    }
    else{
        // no record yet. Let's make one.
        channelRecordFilePtrTemp = std::make_shared<channelRecordFile>();

        channelRecordFilePtrTemp->pathToFile = logFilePath;
        channelRecordFilePtrTemp->fStream.exceptions(std::ios_base::failbit | std::ios_base::badbit);
    }

    if(!channelRecordFilePtrTemp->fStream.is_open()){

        std::filesystem::create_directories(logFileDir);

        if(!std::filesystem::exists(logFilePath)){
            // create the file
            channelRecordFilePtrTemp->fStream.open(logFilePath, std::ios::out);

            try {
                channelRecordFilePtrTemp->fStream.close();
            }
            catch (std::exception &e){
                APATE_LOG_WARN("Failed to close file {} - {}",
                               logFilePath.string(),
                               e.what());
            }
            catch(...){
                APATE_LOG_WARN("Failed to close file {}", logFilePath.string());
            }
        }

        // re-open it in input/output mode
        channelRecordFilePtrTemp->fStream.open(channelRecordFilePtrTemp->pathToFile, std::ios::binary|std::ios::in|std::ios::out);
        channelRecordFilePtrTemp->fStream.seekg(0, std::ios::end);
        auto [it, _] = m_channelLogFiles.insert(std::pair{ channelId, channelRecordFilePtrTemp });
    }

    return channelRecordFilePtrTemp;
}
}
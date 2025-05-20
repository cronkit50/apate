#include "messagearchiver.hpp"

#include "common/util.hpp"
#include "embed/embed.hpp"
#include "log/log.hpp"

#include <format>

static const size_t MIN_MESSAGE_LEN_FOR_EMBEDDING = 10;

static std::string GenerateEmbeddingString(const dpp::message& message){

    // keep the time as part of the embedding for temporal awareness
    std::string embeddingString = std::format("{} {} {}",
                                              SnowflakeFriendly (message.id),
                                              message.author.global_name,
                                              message.content);

    return embeddingString;
}


namespace discord{

messageArchiver::messageArchiver(const std::filesystem::path& persistenceDir) : m_persistenceDir (persistenceDir) {
}


messageArchiver::messageArchiver(void){
    // default

    m_persistenceDir = GetDirectory(DIRECTORY_EXE);
}

void messageArchiver::SetPersistenceDir(const std::filesystem::path& dir){
    m_persistenceDir = dir;
    m_persistenceDir.remove_filename();


}

void messageArchiver::RecordLatestMessage(const dpp::message& message){
    dpp::message_map map;
    map.emplace(message.id, message);

    BatchRecordLatestMessages(message.guild_id, message.channel_id, map);

}

void messageArchiver::BatchRecordLatestMessages(const dpp::snowflake guildId, const dpp::snowflake channelId, const dpp::message_map& messages){

    auto& persistenceWrapper = GetGuildPersistence(guildId);
    std::unique_lock lock(persistenceWrapper.mutex);
    persistenceWrapper.persistence.RecordLatestMessages(messages);

    std::vector<std::string>    embeddingsToGenerate;
    std::vector<dpp::snowflake> messageIDsToGenerate;

    for (const auto &[messageID, message] : messages){

        if (message.content.size () < MIN_MESSAGE_LEN_FOR_EMBEDDING){
            continue;
        }

        if(!persistenceWrapper.persistence.HasEmbedding(channelId, messageID)){
            embeddingsToGenerate.push_back(GenerateEmbeddingString(message));
            messageIDsToGenerate.push_back(messageID);
        }
    }
    lock.unlock();
    if(!embeddingsToGenerate.empty()){
        auto future = TransformSentences(embeddingsToGenerate);

        std::future_status rc = std::future_status::ready;
        if ((rc = future.wait_for(std::chrono::seconds(120))) != std::future_status::ready){
            APATE_LOG_WARN("Failed to get '{}' embeddings for channel {} - {}",
                           embeddingsToGenerate.size(),
                           channelId.str(),
                           (int)rc);
        }
        else{
            auto embeddings = future.get();
            if(embeddings.size() != embeddingsToGenerate.size()){
                APATE_LOG_WARN("Embeddings size = '{}' mismatches inputs {} for channel {}",
                               embeddings.size(),
                               embeddingsToGenerate.size(),
                               channelId.str());
            }
            else{
                lock.lock();
                for(size_t ii = 0; ii<embeddings.size(); ii++){
                    persistenceWrapper.persistence.SaveEmbedding(channelId,
                                                                 messageIDsToGenerate[ii],
                                                                 embeddings[ii]);
                }
            }
        }
    }

}

size_t messageArchiver::CountContinousMessages(const dpp::snowflake guildId, const dpp::snowflake channelId, const dpp::snowflake since){
    auto& persistenceWrapper = GetGuildPersistence(guildId);
    std::lock_guard lock(persistenceWrapper.mutex);
    return persistenceWrapper.persistence.CountContinuousMessages(channelId, since);
}

dpp::snowflake messageArchiver::GetOldestContinuousTimestamp(const dpp::snowflake guildId, const dpp::snowflake channelId, const dpp::snowflake since){
    auto& persistenceWrapper = GetGuildPersistence(guildId);
    std::lock_guard lock(persistenceWrapper.mutex);
    return persistenceWrapper.persistence.GetOldestContinuousTimestamp(channelId, since);
}

messageRecord messageArchiver::FindMessage(const dpp::snowflake guildId, const dpp::snowflake channelId, const dpp::snowflake messageId){

    messageRecord msg;

    auto& persistenceWrapper = GetGuildPersistence(guildId);
    std::lock_guard lock(persistenceWrapper.mutex);
    persistenceWrapper.persistence.FindMessage (channelId, messageId, msg);

    return msg;

}


std::vector<messageRecord> messageArchiver::GetContinousMessages(const dpp::snowflake guildId,
                                                                 const dpp::snowflake channelId,
                                                                 const size_t         numMessages){
    auto& persistenceWrapper = GetGuildPersistence(guildId);
    std::lock_guard lock(persistenceWrapper.mutex);
    return persistenceWrapper.persistence.GetContinousMessagesByChannel(channelId, numMessages);
}

std::vector<messageRecord> messageArchiver::GetContextRelevantMessages(const dpp::message& message, const size_t numMessages){
    std::vector<messageRecord> relevant;
    auto future = TransformSentences({ message.content });

    std::future_status rc = std::future_status::ready;
    if ((rc = future.wait_for(std::chrono::seconds(30))) != std::future_status::ready){
        APATE_LOG_WARN("Failed to get embeddings for channel {} - {}",
                       message.channel_id.str(),
                       message.content,
                       (int)rc);
    }
    else{
        auto embeddedVector = future.get();

        if (embeddedVector.size () != 1){
            APATE_LOG_WARN("Embedding vector size not expected = {}",
                           embeddedVector.size ());
        }
        else{
            auto faiss = GetFaiss (message.guild_id, message.channel_id);
            std::lock_guard lock(faiss->mutex);

            std::vector<faiss::idx_t> indexes(numMessages);
            std::vector<float> similarityScores(numMessages);

            faiss->flatFaiss.search(1, // thing to query
                                    embeddedVector[0].data (),
                                    numMessages,
                                    similarityScores.data(),
                                    indexes.data());

            for(size_t ii = 0; ii < indexes.size (); ii++){
                auto result = indexes[ii];
                auto similarity = similarityScores[ii];

                auto messageId = faiss->faissSnowflakes[result];
                auto foundMsg = FindMessage(message.guild_id, message.channel_id, messageId);

                if(!foundMsg.snowflake.empty()){
                    relevant.push_back(foundMsg);

                }
            }
        }
    }
    return relevant;
}

messageArchiver::serverPersistenceWrapper& discord::messageArchiver::GetGuildPersistence(const dpp::snowflake& guildID){
    std::lock_guard lock(m_persistenceDictMtx);

    if(m_persistenceByGuild.count(guildID)<=0){
        std::filesystem::path guildDir = m_persistenceDir;
        guildDir.append(guildID.str() + "\\");

        serverPersistence persistence;
        persistence.SetBaseDirectory(guildDir);

        auto [it,_] = m_persistenceByGuild.emplace(guildID, std::move(persistence));
        return(it->second);

    }
    else{
        return m_persistenceByGuild[guildID];
    }
}
std::shared_ptr<messageArchiver::faissIndexWrapper> messageArchiver::GetFaiss(const dpp::snowflake& guildID, const dpp::snowflake channelId){
    std::lock_guard lock(m_faissDictMtx);
    if (m_faissByChannel.count(channelId) > 0){
        return m_faissByChannel[channelId];
    }
    else{
        // make a new FAISS



        std::shared_ptr<faissIndexWrapper> newFaiss = std::make_shared<faissIndexWrapper>();
        std::vector<embeddingRecord> embeddings;

        auto& persistenceWrapper = GetGuildPersistence(guildID);
        {
            std::lock_guard lock(persistenceWrapper.mutex);
            embeddings = persistenceWrapper.persistence.GetVectorEmbeddings(channelId);
        }

        for (const auto embedding : embeddings){

            // remember the snowflakes for later
            newFaiss->faissSnowflakes.push_back(embedding.messageId);
            newFaiss->flatFaiss.add(1, embedding.embedding.data());
        }
        
        newFaiss->flatFaiss.hnsw.efSearch = 500;
        m_faissByChannel.emplace(channelId, newFaiss);
        return newFaiss;
    }
}
}

#include "discordbot.hpp"

#include "log/log.hpp"
#include "common/util.hpp"

const char *askChatGptCommand = "askchatgpt";

namespace discord
{
discordBot::discordBot(const std::string& discordAPIToken)
    : m_api (discordAPIToken),
      m_cluster(discordAPIToken, dpp::i_default_intents | dpp::i_message_content)
{
    auto messageCreateHandler = std::bind(&discordBot::HandleMessageEvent,   this, std::placeholders::_1);
    auto readyHandler         = std::bind(&discordBot::HandleOnReady,        this, std::placeholders::_1);
    auto commandHandler       = std::bind(&discordBot::HandleOnSlashCommand, this, std::placeholders::_1);

    m_cluster.on_message_create(messageCreateHandler);
    m_cluster.on_ready(readyHandler);
    m_cluster.on_slashcommand(commandHandler);
}


void discordBot::Start(void){
    std::lock_guard lock (m_botThreadWaitMtx);

    m_botStartedOK      = false;
    m_botThreadWaitFlag = false;
    // to-do, handle bot-restarts.

    try {
        // let dpp call our on_ready callback to set the return flag
        this->m_cluster.start(dpp::st_return);
    }
    catch (...){
        // set the return flag ourselves and wake up any waiters
        m_botStartedOK      = false;
        m_botThreadWaitFlag = true;
        m_botThreadWaitCV.notify_all();
    }
}


discordBot::~discordBot(){
    m_cluster.shutdown();
}


bool discordBot::WaitForStart(){
    std::unique_lock lock(m_botThreadWaitMtx);
    m_botThreadWaitCV.wait(lock, [&]()->bool{return (m_botThreadWaitFlag);});
    return m_botStartedOK;
}

void discordBot::SetWorkingDir(const std::filesystem::path dir){
    m_workingDir = dir;
    m_workingDir.remove_filename();
}


void discordBot::SetChatGPT(std::unique_ptr<openai::chatGPT> &&chatGPT){
    if (!chatGPT){
        return;
    }

    if (m_chatGPT){
        m_chatGPT.reset();
    }

    m_chatGPT = std::move(chatGPT);
}


void discordBot::HandleOnSlashCommand(const dpp::slashcommand_t& event){

    const dpp::interaction &command = event.command;

    if(command.get_command_name () == askChatGptCommand){
        std::string query = std::get<std::string>(event.get_parameter("query"));


        if(m_chatGPT){
            openai::chatGPTPrompt prompt;
            prompt.systemPrompt = "You are a helpful AI, but be brief in your answers.";
            prompt.request = query;
            prompt.model.modelValue = m_model;

            std::string chatGPTQuery = event.command.usr.username + " asks: " + query;
            event.reply(chatGPTQuery);

            std::thread async([this,prompt,event](void){
                try{
                    std::future<openai::chatGPTResponse> future = m_chatGPT->AskChatGPTAsync(prompt);

                    openai::chatGPTResponse response = future.get();
                    if(response.responseOK){
                        dpp::message msg(event.command.channel_id, std::get<openai::chatGPTOutputMessage>(response.outputs[0].content).message);
                        m_cluster.message_create(msg);
                    }
                } catch (const std::exception &e){
                    APATE_LOG_WARN_AND_RETHROW (e);
                }
             });

            async.detach();
        }
    }
}

void discordBot::HandleOnReady(const dpp::ready_t& event){
    // register our commands
    dpp::slashcommand askChatGPT(askChatGptCommand, "wastes Rocket50's money to ask chatGPT a question", m_cluster.me.id);
    askChatGPT.add_option (dpp::command_option{ dpp::co_string, "query",  "ask i guess", true });
    m_cluster.global_bulk_command_create({ askChatGPT });

    m_botStartedOK      = true;
    m_botThreadWaitFlag = true;
    m_botThreadWaitCV.notify_all();


}


void discordBot::HandleMessageEvent(const dpp::message_create_t& event){
    try {
        auto& persistenceWrapper = GetPersistence(event.msg.guild_id);

        std::lock_guard lock(persistenceWrapper.mutex);
        persistenceWrapper.persistence.RecordMessage(event.msg);

    }
    catch (const std::exception &e){
        APATE_LOG_WARN("Failed to record message event - {}", e.what());
    }
    catch(...){
        APATE_LOG_WARN("Failed to record message event - unknown exception");
    }
}

void discordBot::StartArchiving(const dpp::ready_t& event){
    return;
    for(auto guildId:event.guilds){
        // TO DO, USE THREAD POOL
        std::thread t([this, guildId](){
            std::promise<dpp::channel_map> promise;
            auto future = promise.get_future();

            m_cluster.channels_get(guildId,
                [&promise](const dpp::confirmation_callback_t& callback){
                    if(callback.is_error()){
                        APATE_LOG_WARN("Failed to get channels for guild - {}", callback.get_error().message);
                        promise.set_value({});
                    }
                    else if(!std::holds_alternative<dpp::channel_map>(callback.value)){
                        APATE_LOG_WARN("Unexpected result from getting channels - {}", callback.get_error().message);
                        promise.set_value({});
                    }
                    else{
                        promise.set_value(std::get<dpp::channel_map>(callback.value));
                    }
                });

            // wait for the channels to be retrieved
            auto rc = future.wait_for(std::chrono::seconds(10));
            if(rc == std::future_status::timeout){
                APATE_LOG_WARN("Timed out waiting for channels for guild: {}", guildId.str());
                return;
            }
            auto channels = future.get();

            if(channels.empty()){
                APATE_LOG_WARN("No channels found for guild: {}", guildId.str());
                return;
            }

            for(auto& [_, channel] : channels){
                if(channel.get_type() != dpp::channel_type::CHANNEL_TEXT){
                    continue;
                }
                std::promise<dpp::message_map> messagePromise;
                auto messagesFuture = messagePromise.get_future();

                m_cluster.messages_get(channel.id, 0, SnowflakeNow (), 0,
                                       m_OnStartFetchAmount,
                    [&messagePromise](const dpp::confirmation_callback_t& callback){
                        if(callback.is_error()){
                            APATE_LOG_WARN("Failed to get message for channel - {}", callback.get_error().message);
                            messagePromise.set_value({});
                        }
                        else if(!std::holds_alternative<dpp::message_map>(callback.value)){
                            APATE_LOG_WARN("Unexpected result from getting channel messages - {}", callback.get_error().message);
                            messagePromise.set_value({});
                        }
                        else{
                            messagePromise.set_value(std::get<dpp::message_map>(callback.value));
                        }
                    });

                auto rc = messagesFuture.wait_for(std::chrono::seconds(10));
                if(rc == std::future_status::timeout){
                    APATE_LOG_WARN("Timed out waiting for messages for channel: {} - {}",
                                   channel.id.str (),
                                   channel.name);
                    continue;
                }
                auto& persistence = GetPersistence(guildId);
                std::lock_guard lock(persistence.mutex);
                persistence.persistence.RecordMessages(messagesFuture.get());


            };
        });
        // to do, this is wrong and is undefined if the class goes out of scope
        t.detach();
    }
}

discordBot::serverPersistenceWrapper& discordBot::GetPersistence(const dpp::snowflake& guildID){
    std::lock_guard lock(m_persistenceDictMtx);

    if(m_persistenceByGuild.count(guildID)<=0){
        std::filesystem::path guildDir = m_workingDir;
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

}
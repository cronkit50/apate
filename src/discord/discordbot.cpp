#include "discordbot.hpp"

#include "log/log.hpp"

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

void discordBot::SetPersistence(discord::serverPersistence&& persistence){
    m_persistence = std::forward<discord::serverPersistence>(persistence);
}

void discordBot::SetChatGPT(openai::chatGPT* chatGPT){
    if (!chatGPT){
        return;
    }

    if (m_chatGPT){
        delete m_chatGPT;
    }

    m_chatGPT = chatGPT;
}


void discordBot::HandleOnSlashCommand(const dpp::slashcommand_t& event){

    const dpp::interaction &command = event.command;

    if(command.get_command_name () == askChatGptCommand){
        std::string query = std::get<std::string>(event.get_parameter("query"));


        if(m_chatGPT){
            openai::chatGPTPrompt prompt;
            prompt.systemPrompt = "You are a helpful AI, but be brief in your answers.";
            prompt.request = query;
            prompt.model.modelValue   = "o4-mini-2025-04-16";

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
                    APATE_LOG_INFO_AND_RETHROW (e);
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
    std::lock_guard lock(m_eventCallbackMtx);
    m_persistence.RecordMessageEvent(event);
}

}
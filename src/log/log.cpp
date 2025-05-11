#include "log.hpp"

#include <future>
#include <sstream>
#include <vector>
#include <map>
#include <mutex>

std::string logMessage::FormattedMessage() const{
    using namespace std::chrono;

    // get the factional bit
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(timeStamp.time_since_epoch()).count() % 1000;

    std::string timePart  = std::format("{:%Y-%m-%d %H:%M:%S}",
                                        time_point_cast<std::chrono::seconds>(timeStamp));

    std::stringstream threadID_SS;
    threadID_SS << threadID;

    std::string formatted = std::format("[{}] {}.{:03} {:<15} {:<5} (T{}) {}",
                                        LogSeverityString(severity),
                                        timePart,
                                        milliseconds,
                                        fileName,
                                        lineNumber,
                                        threadID_SS.str(),
                                        message);
    return formatted;
}

Logger::Logger(){
    auto threadFunc = std::bind(&Logger::HandleLogQ, this);

    m_logQHandler = std::thread(threadFunc);
}

Logger::~Logger(){
    m_shutdownFlag = true;
    m_logCV.notify_all();

    if(m_logQHandler.joinable()){
        m_logQHandler.join();
    }
}

void Logger::AddCallback(const logCallback& cb){
    std::lock_guard lock (m_callbacksMtx);

    if(!cb){
        APATE_LOG_DEBUG("callback was null");
    }
    else{
        m_callbacks.push_back(cb);
    }
}

void Logger::AddLog(const logMessage& log){
    std::lock_guard lock (m_logQMtx);
    m_logQ.push(log);
    m_logCV.notify_all();
}

Logger& Logger::GetInstance(){
    static Logger logger;
    return logger;
}

void Logger::HandleLogQ(void){

    std::unique_lock cvLock(m_logQMtx);

    while(!m_shutdownFlag || !m_logQ.empty()){
        m_logCV.wait(cvLock, [this](){return !m_logQ.empty() || m_shutdownFlag; });

        // flush the queue of all logs before shutting down for good
        while(!m_logQ.empty()){
            logMessage thisLog = std::move(m_logQ.front());
            m_logQ.pop();
            cvLock.unlock();

            // avoid lock contention and copy the callbacks.
            // this shouldn't be a huge list.
            std::unique_lock callbackQLock(m_callbacksMtx);
            auto callbacksCopy = m_callbacks;
            callbackQLock.unlock();

            std::string formattedMessage = thisLog.FormattedMessage();

            for(const auto& cb : callbacksCopy){
                try{
                    cb(thisLog, formattedMessage);
                // avoid recursive logging issues if cb fails
                } catch(std::exception& e){
                    std::cerr << "Logging callback threw an exception: - " << e.what();
                } catch(...){
                    std::cerr << ("Logging callback threw an unknown exception");
                }
            }
            cvLock.lock();
        }
    }
}

const char* LogSeverityString(const log_severity severity){
    static const std::map<log_severity, const char*> dict {
        { LOG_SEVERITY_INFO,   "INFO" },
        { LOG_SEVERITY_DOMAIN, "DOMAIN" },
        { LOG_SEVERITY_WARN,   "WARN" },
        { LOG_SEVERITY_SEVERE, "SEVERE" }
    };
    auto it = dict.find(severity);

    return (it == dict.end()) ? "UNKNOWN" : dict.at(severity);
}

void AddOnLog(const logCallback& callback){
    Logger::GetInstance().AddCallback(callback);
}

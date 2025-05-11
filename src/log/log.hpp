#ifndef LOG_HPP
#define LOG_HPP

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <format>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <utility>


enum log_severity{
    LOG_SEVERITY_INFO = 0,
    LOG_SEVERITY_DOMAIN,
    LOG_SEVERITY_WARN,
    LOG_SEVERITY_SEVERE
};

static inline bool SeverityOK(log_severity severity) {
    return severity >= LOG_SEVERITY_INFO && severity <= LOG_SEVERITY_SEVERE;
}

typedef std::chrono::time_point<std::chrono::system_clock,
                                std::chrono::duration<double, std::ratio<1>>> logTimestamp;

struct logMessage{
    log_severity severity   = LOG_SEVERITY_INFO;
    size_t       lineNumber = 0;
    std::string  file;
    std::string  message;
    logTimestamp timeStamp;
    std::thread::id threadID;

    std::string FormattedMessage() const;
};


typedef std::function<void(const logMessage&, const std::string&)> logCallback;


#define APATE_LOG_INFO(format, ...) LogMessage(__FILE__, __LINE__, LOG_SEVERITY_INFO, format, ##__VA_ARGS__);
#define APATE_LOG_DOMAIN(format, ...) LogMessage(__FILE__, __LINE__, LOG_SEVERITY_DOMAIN, format, ##__VA_ARGS__);
#define APATE_LOG_WARN(format, ...) LogMessage(__FILE__, __LINE__, LOG_SEVERITY_WARN, format, ##__VA_ARGS__);
#define APATE_LOG_SEVERE(format, ...) LogMessage(__FILE__, __LINE__, LOG_SEVERITY_SEVERE, format, ##__VA_ARGS__);

class Logger{
public:
    Logger();
    ~Logger();

    Logger(Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&) = delete;
    Logger& operator=(Logger&&) = delete;




    void AddCallback(const logCallback& cb);
    void AddLog(const logMessage& log);


    // Singleton instance
    static Logger& GetInstance();


private:
    void HandleLogQ (void);

    std::atomic<bool> m_shutdownFlag = false;

    std::thread m_logQHandler;
    std::mutex  m_logQMtx;
    std::condition_variable m_logCV;
    std::queue<logMessage> m_logQ;

    std::mutex  m_callbacksMtx;
    std::vector<logCallback> m_callbacks;
};

template <typename... Args>
void LogMessage(const char* file, size_t line, const log_severity severity, const char *fmt, Args&&... args){
    if(!file){
        std::cerr << "File was null";
        return;
    }
    if(!fmt){
        std::cerr << "Format was null. Called by " << file << " line " << line;
        return;
    }

    logMessage log;
    log.file = file;
    log.lineNumber = line;
    log.severity = severity;
    log.timeStamp = std::chrono::system_clock::now();
    log.threadID  = std::this_thread::get_id();

    try{
        log.message = std::vformat(fmt, std::make_format_args(args...));
    }
    catch (const std::exception &e){
        // format seems to failed. maybe the caller didn't give enough arguments?
        log.message = std::format("Logging failed for format: '{}' - {}. Number of arguments provided: {}",
                                  fmt,
                                  e.what(),
                                  sizeof...(args));
    }

    Logger::GetInstance().AddLog(log);

    }

const char *LogSeverityString(const log_severity severity);


void AddOnLog(const logCallback &callback);

#endif
#include <iostream>
#include <fstream>
#include <ctime>
#include <mutex>

enum class LogLevel { DEBUG, INFO, WARNING, ERROR };

class Logger {
public:
    Logger(const std::string& filename = "log.txt", LogLevel level = LogLevel::INFO)
        : m_filename(filename), m_logLevel(level) {}

    void log(LogLevel level, const std::string& message) {
        if (level >= m_logLevel) {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::ofstream ofs(m_filename, std::ios::app);
            ofs << "[" << getCurrentTime() << "] ";
            ofs << "[" << logLevelToString(level) << "] ";
            ofs << message << std::endl;
            ofs.close();
        }
    }

    void setLogLevel(LogLevel level) { m_logLevel = level; }

private:
    std::string getCurrentTime() {
        std::time_t now = std::time(nullptr);
        char timeStr[64];
        std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        return std::string(timeStr);
    }

    std::string logLevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG:   return "DEBUG";
            case LogLevel::INFO:    return "INFO";
            case LogLevel::WARNING: return "WARNING";
            case LogLevel::ERROR:   return "ERROR";
        }
        return "UNKNOWN";
    }

private:
    std::string m_filename;
    LogLevel m_logLevel;
    std::mutex m_mutex;
};


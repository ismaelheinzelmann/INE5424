#pragma once
#ifndef LOGGER_H
#define LOGGER_H
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <string>

enum class LogLevel {
    INFO,
    DEBUG,
    WARNING,
    ERROR,
    FATAL,
    UNKNOWN
};

class Logger {
public:
    static bool setLogLevel(const std::string& level_str) {
        LogLevel level = stringToLogLevel(level_str);
        if (level == LogLevel::UNKNOWN) {
            return false;
        }
        current_log_level = level;
        return true;
    }

    static void log(const std::string& message, LogLevel level = LogLevel::INFO) {
        if (level < current_log_level) {
            return;
        }

        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);

        std::tm local_time;
#if defined(_WIN32)
#else
        localtime_r(&now_c, &local_time);
#endif
        std::cout << std::put_time(&local_time, "[%Y-%m-%d %H:%M:%S] ");
        std::cout << "[" << logLevelToString(level) << "] " << message << std::endl;
    }

private:
    static LogLevel current_log_level;

    static std::string logLevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::INFO:    return "INFO";
            case LogLevel::DEBUG:   return "DEBUG";
            case LogLevel::WARNING:  return "WARNING";
            case LogLevel::ERROR:    return "ERROR";
            case LogLevel::FATAL:    return "FATAL";
            default:                return "UNKNOWN";
        }
    }

    static LogLevel stringToLogLevel(const std::string& level_str) {
        if (level_str == "INFO") return LogLevel::INFO;
        if (level_str == "DEBUG") return LogLevel::DEBUG;
        if (level_str == "WARNING") return LogLevel::WARNING;
        if (level_str == "ERROR") return LogLevel::ERROR;
        if (level_str == "FATAL") return LogLevel::FATAL;
        return LogLevel::UNKNOWN;
    }
};
#endif //LOGGER_H

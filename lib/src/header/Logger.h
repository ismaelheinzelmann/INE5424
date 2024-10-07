#pragma once
#ifndef LOGGER_H
#define LOGGER_H
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <string>

enum class LogLevel { INFO, DEBUG, WARNING, ERROR, FATAL, NONE };

// ANSI color codes
const std::string COLOR_RESET = "\033[0m";
const std::string COLOR_INFO = "\033[32m"; // Green
const std::string COLOR_DEBUG = "\033[36m"; // Cyan
const std::string COLOR_WARNING = "\033[33m"; // Yellow
const std::string COLOR_ERROR = "\033[31m"; // Red
const std::string COLOR_FATAL = "\033[41m"; // Red background

class Logger {
public:
	static bool setLogLevel(const std::string &level_str) {
		LogLevel level = stringToLogLevel(level_str);
		if (level == LogLevel::NONE) {
			return false;
		}
		current_log_level = level;
		return true;
	}

	static void log(const std::string &message, LogLevel level) {
		if (level > current_log_level) {
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
		std::cout << getColorForLogLevel(level) << "[" << logLevelToString(level) << "] " << COLOR_RESET << message
				  << std::endl;
	}

private:
	static LogLevel current_log_level;

	static std::string logLevelToString(LogLevel level) {
		switch (level) {
		case LogLevel::INFO:
			return "INFO";
		case LogLevel::DEBUG:
			return "DEBUG";
		case LogLevel::WARNING:
			return "WARNING";
		case LogLevel::ERROR:
			return "ERROR";
		case LogLevel::FATAL:
			return "FATAL";
		default:
			return "NONE";
		}
	}

	static std::string getColorForLogLevel(LogLevel level) {
		switch (level) {
		case LogLevel::INFO:
			return COLOR_INFO;
		case LogLevel::DEBUG:
			return COLOR_DEBUG;
		case LogLevel::WARNING:
			return COLOR_WARNING;
		case LogLevel::ERROR:
			return COLOR_ERROR;
		case LogLevel::FATAL:
			return COLOR_FATAL;
		default:
			return COLOR_RESET;
		}
	}

	static LogLevel stringToLogLevel(const std::string &level_str) {
		if (level_str == "INFO")
			return LogLevel::INFO;
		if (level_str == "DEBUG")
			return LogLevel::DEBUG;
		if (level_str == "WARNING")
			return LogLevel::WARNING;
		if (level_str == "ERROR")
			return LogLevel::ERROR;
		if (level_str == "FATAL")
			return LogLevel::FATAL;
		return LogLevel::NONE;
	}
};

#endif // LOGGER_H

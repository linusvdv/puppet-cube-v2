#include "logger.h"
#include <mutex>


LoggerLevel Logger::logger_level = LoggerLevel::kMemory;
std::mutex Logger::log_mutex;


void Logger::SetLoggerLevel (LoggerLevel level) {
    std::lock_guard<std::mutex> lock(log_mutex);
    logger_level = level;
}

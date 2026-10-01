#include <mutex>

#include "cube.hpp"
#include "logger.hpp"


LoggerLevel Logger::logger_level = LoggerLevel::kMemory;
std::mutex Logger::log_mutex;


void Logger::SetLoggerLevel (LoggerLevel level) {
    std::lock_guard<std::mutex> lock(log_mutex);
    logger_level = level;
}


std::ostringstream& operator<<(std::ostringstream& oss, Rotations rotation) {
    const std::vector<std::string> rotation_names = {
        "R", "R'", "L", "L'", "U", "U'", "D", "D'",
        "F", "F'", "B", "B'", "M", "M'", "E", "E'", "S", "S'"};
    oss << rotation_names[int(rotation)];
    return oss;
}

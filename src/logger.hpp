#pragma once

#include <array>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <source_location>
#include <sstream>
#include <stack>
#include <string_view>
#include <type_traits>
#include <vector>

#include "cube.hpp"
#include "nadeau.h"


enum Rotations : uint8_t;
std::ostringstream& operator<<(std::ostringstream& oss, Rotations rotation);


enum class LoggerLevel {
    kCriticalError,
    kError,
    kWarning,
    kInfo,
    kAll,
    kExtra,
    kMemory
};


struct TextFormat {
    int color = 0;
    std::string_view level_name;
};


constexpr std::array<TextFormat, 7> kTextFormat = {{
    {41, "CRITICAL ERROR: "},
    {31, "ERROR: "},
    {33, "WARNING: "},
    {32, "INFO: "},
    {0, ""},
    {90, "EXTRA: "},
    {90, "MEMORY: "}
}};


class Logger {
    public:
        static void SetLoggerLevel (LoggerLevel level);

        template<typename... Args>
        static void Log (LoggerLevel level, const std::source_location& source_location = std::source_location::current(), Args&&... args);

    private:
        static LoggerLevel logger_level;
        static std::mutex log_mutex;
};


template<typename T>
struct SkipSpace {
    T value;
    constexpr explicit SkipSpace(T&& val) : value(std::forward<T>(val)) {}
    constexpr explicit SkipSpace(const T& val) : value(val) {}
};
template<typename T>
SkipSpace(T&&) -> SkipSpace<std::decay_t<T>>;
template<typename T, size_t N>
SkipSpace(T (&)[N]) -> SkipSpace<const T*>;
template<typename T>
inline constexpr bool kIsSkippedSpace = false;
template<typename T>
inline constexpr bool kIsSkippedSpace<SkipSpace<T>> = true;

template<typename... Args>
void Logger::Log (LoggerLevel level, const std::source_location& source_location, Args&&... args) {
    if (level > logger_level) {
        return;
    }

    std::ostringstream oss;

    int level_idx = static_cast<int>(level);
    oss << "\033[" << kTextFormat[level_idx].color << "m";

    // print local time thread-safe
    std::time_t time = std::time(nullptr);
    std::tm tm_time;
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm_time, &time);  // Windows thread-safe localtime
#else
    localtime_r(&time, &tm_time);  // POSIX thread-safe localtime
#endif
    oss << "[" << std::put_time(&tm_time, "%a %b %d %H:%M:%S %Y") << "] ";

    oss << kTextFormat[level_idx].level_name;

    if (level <= LoggerLevel::kWarning) {
        const std::string_view file = source_location.file_name();
        const std::string_view file_name = file.substr(file.find_last_of("/\\") + 1);
        oss << "[" << file_name << ":" << source_location.line() << "] In function " << source_location.function_name() << " --- ";
    }

    if (level == LoggerLevel::kMemory) {
        oss << "Current: " << getCurrentRSS() / 1024 / 1024 << " MB \tMax: " << getPeakRSS() / 1024 / 1024 << " MB";  // NOLINT
    }
    else if constexpr (sizeof...(args) == 0) {
        Log(LoggerLevel::kError, source_location, "no arguments passed to Log");
        return;
    }
    else {
        (([&] {
            if constexpr (kIsSkippedSpace<std::decay_t<Args>>) {
                oss << args.value;
            }
            else if constexpr (std::is_same_v<std::remove_cvref_t<decltype(args)>, std::stack<Rotations>>) {
                std::stack<Rotations> rotations_cpy = args;
                while (!rotations_cpy.empty()) {
                    oss << rotations_cpy.top() << ' ';
                    rotations_cpy.pop();
                }
            }
            else if constexpr (std::is_same_v<std::remove_cvref_t<decltype(args)>, std::vector<Rotations>>) {
                for (Rotations rotation : args) {
                    oss << rotation << ' ';
                }
            }
            else {
                oss << args << ' ';
            }
        }()), ...);
    }
    oss << "\033[0m" << "\n";
    std::cout << oss.str() << std::flush;

    if (level == LoggerLevel::kCriticalError) {
        exit(-1);
    }
}


// --- Helper Macros for Easy Logging ---
#define LOG_CRITICAL(...) Logger::Log(LoggerLevel::kCriticalError, std::source_location::current(), __VA_ARGS__)
#define LOG_ERROR(...)    Logger::Log(LoggerLevel::kError, std::source_location::current(), __VA_ARGS__)
#define LOG_WARNING(...)  Logger::Log(LoggerLevel::kWarning, std::source_location::current(), __VA_ARGS__)
#define LOG_INFO(...)     Logger::Log(LoggerLevel::kInfo, std::source_location::current(), __VA_ARGS__)
#define LOG_ALL(...)      Logger::Log(LoggerLevel::kAll, std::source_location::current(), __VA_ARGS__)
#define LOG_EXTRA(...)    Logger::Log(LoggerLevel::kExtra, std::source_location::current(), __VA_ARGS__)
#define LOG_MEMORY()   Logger::Log(LoggerLevel::kMemory, std::source_location::current())

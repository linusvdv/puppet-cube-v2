#pragma once

#include <string>


class ErrorHandler {
public:
    enum Level {
        kCriticalError,
        kError,
        kWarning,
        kInfo,
        kAll,
        kExtra
    };

    Level error_level;

    ErrorHandler(Level level) {
        SetErrorLevel(level);
    }

    void SetErrorLevel(Level level) {
        error_level = level;
    }

    void Handle(Level level, std::string file, std::string message) const;
};

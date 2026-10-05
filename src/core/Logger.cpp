#include "core/Logger.h"

#include <iostream>
#ifndef __EMSCRIPTEN__
#include <filesystem>
#include <system_error>
#endif

Logger::Logger(const std::string& path) : Logger(std::clog, path) {}

Logger::Logger(std::ostream& console, const std::string& path) : console_(console) {
#ifndef __EMSCRIPTEN__
    if (!path.empty()) {
        const auto parent = std::filesystem::path(path).parent_path();
        std::error_code error;
        if (!parent.empty()) {
            std::filesystem::create_directories(parent, error);
        }
        if (!error) {
            file_.open(path, std::ios::app);
        }
        if (error || !file_) {
            console_ << "[WARN] Cannot open log file: " << path << '\n';
        }
    }
#else
    (void)path;
#endif
}

void Logger::log(LogLevel level, const std::string& message) {
    const char* label = "INFO";
    switch (level) {
        case LogLevel::Debug:
            label = "DEBUG";
            break;
        case LogLevel::Info:
            label = "INFO";
            break;
        case LogLevel::Warn:
            label = "WARN";
            break;
        case LogLevel::Error:
            label = "ERROR";
            break;
    }
    console_ << '[' << label << "] " << message << '\n';
    console_.flush();
    if (file_) {
        file_ << '[' << label << "] " << message << '\n';
        file_.flush();
    }
}

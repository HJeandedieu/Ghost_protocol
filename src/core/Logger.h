#pragma once

#include <fstream>
#include <ostream>
#include <string>

enum class LogLevel { Debug, Info, Warn, Error };

class Logger {
   public:
    explicit Logger(const std::string& path = "logs/ghost.log");
    Logger(std::ostream& console, const std::string& path);
    void log(LogLevel level, const std::string& message);

   private:
    std::ostream& console_;
    std::ofstream file_;
};

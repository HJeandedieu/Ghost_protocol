#include <gtest/gtest.h>

#include <fstream>
#include <sstream>

#include "TestFiles.h"
#include "core/Logger.h"

TEST(Logger, CreatesParentDirectoryAndFlushesToConsoleAndFile) {
    TestFiles files;
    std::ostringstream console;
    Logger logger(console, files.path("logs/ghost.log"));
    logger.log(LogLevel::Info, "startup");
    logger.log(LogLevel::Warn, "fallback");
    std::ifstream log(files.path("logs/ghost.log"));
    std::ostringstream content;
    content << log.rdbuf();
    EXPECT_EQ(content.str(), "[INFO] startup\n[WARN] fallback\n");
    EXPECT_EQ(console.str(), content.str());
}

TEST(Logger, UnwritableFileKeepsConsoleLoggingAvailable) {
    TestFiles files;
    const auto parentFile = files.write("not-a-directory", "file");
    std::ostringstream console;
    Logger logger(console, parentFile + "/ghost.log");
    logger.log(LogLevel::Error, "still running");
    EXPECT_NE(console.str().find("[WARN] Cannot open log file"), std::string::npos);
    EXPECT_NE(console.str().find("[ERROR] still running"), std::string::npos);
}

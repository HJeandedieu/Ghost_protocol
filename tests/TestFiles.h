#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

class TestFiles {
   public:
    TestFiles() {
        path_ = std::filesystem::temp_directory_path() /
                ("ghost-tests-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(path_);
    }
    ~TestFiles() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }
    TestFiles(const TestFiles&) = delete;
    TestFiles& operator=(const TestFiles&) = delete;
    std::string path(const std::string& name) const { return (path_ / name).string(); }
    std::string write(const std::string& name, const std::string& content) const {
        std::ofstream(path(name)) << content;
        return path(name);
    }

   private:
    std::filesystem::path path_;
};

#include <gtest/gtest.h>

#include <filesystem>
#include <sstream>

#include "TestFiles.h"
#include "core/Assets.h"
#include "core/Logger.h"
#include "world/LevelLoader.h"

namespace {
class WorkingDirectoryGuard {
   public:
    WorkingDirectoryGuard() : original_(std::filesystem::current_path()) {}
    ~WorkingDirectoryGuard() {
        std::error_code error;
        std::filesystem::current_path(original_, error);
    }

   private:
    std::filesystem::path original_;
};
}  // namespace

TEST(Assets, DesktopLaunchFromUnrelatedDirectoryLoadsPackagedBank) {
    TestFiles files;
    WorkingDirectoryGuard restore;
    std::filesystem::current_path(std::filesystem::path(files.path("unused")).parent_path());
    std::ostringstream console;
    Logger logger(console, "");
    // Reproduce the original failure: inherited launch directories have no assets.
    EXPECT_FALSE(LevelLoader::load("assets/levels/gotham_central.json", logger));
    ASSERT_TRUE(Assets::useApplicationDirectory(GP_RUNTIME_TEST_DIRECTORY));
    const auto level = LevelLoader::load("assets/levels/gotham_central.json", logger);
    ASSERT_TRUE(level);
    EXPECT_EQ(level->name, "Gotham Central Bank");
    EXPECT_EQ(level->map.width(), 80);
}

TEST(Assets, InvalidApplicationDirectoryFailsWithoutChangingWorkingDirectory) {
    TestFiles files;
    WorkingDirectoryGuard restore;
    const auto original = std::filesystem::current_path();
    EXPECT_FALSE(Assets::useApplicationDirectory(nullptr));
    EXPECT_FALSE(Assets::useApplicationDirectory(""));
    EXPECT_FALSE(Assets::useApplicationDirectory(files.path("missing").c_str()));
    EXPECT_EQ(std::filesystem::current_path(), original);
}

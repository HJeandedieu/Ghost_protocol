#include "core/Assets.h"

#if defined(_WIN32)
#include <direct.h>
#elif !defined(__EMSCRIPTEN__)
#include <unistd.h>
#endif

bool Assets::useApplicationDirectory(const char* directory) {
#ifdef __EMSCRIPTEN__
    (void)directory;
    return true;
#else
    if (directory == nullptr || directory[0] == '\0') return false;
#ifdef _WIN32
    return _chdir(directory) == 0;
#else
    return chdir(directory) == 0;
#endif
#endif
}

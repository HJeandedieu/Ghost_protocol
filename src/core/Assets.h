#pragma once

class Assets {
   public:
    // Desktop assets are shipped beside the executable. Set this before loading services.
    // Web uses its preloaded virtual filesystem and retains its working directory.
    static bool useApplicationDirectory(const char* directory);
};

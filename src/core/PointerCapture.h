#pragma once

struct CaptureActions {
    bool request = false;
    bool release = false;
    bool pause = false;
    bool discardInput = false;
};

// Platform-independent lifecycle. Browser acquisition is performed only in a gesture callback.
class PointerCapture {
   public:
    CaptureActions sync(bool gameplay, bool focused, bool captured, bool browser);
    bool active() const { return active_; }
    bool wantsCapture() const { return wantsCapture_; }

   private:
    bool active_ = false;
    bool gameplay_ = false;
    bool wantsCapture_ = false;
};

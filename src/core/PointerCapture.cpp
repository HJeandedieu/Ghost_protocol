#include "core/PointerCapture.h"

CaptureActions PointerCapture::sync(bool gameplay, bool focused, bool captured, bool browser) {
    CaptureActions result;
    wantsCapture_ = gameplay && focused;
    result.discardInput = gameplay != gameplay_;
    gameplay_ = gameplay;
    if (!wantsCapture_) {
        result.release = captured;
        result.pause = gameplay && !focused;
        result.discardInput = result.discardInput || active_ || result.pause;
        active_ = false;
        return result;
    }
    if (captured) {
        result.discardInput = result.discardInput || !active_;
        active_ = true;
    } else if (active_) {
        active_ = false;
        result.pause = result.discardInput = true;
    } else {
        result.request = !browser;
    }
    return result;
}

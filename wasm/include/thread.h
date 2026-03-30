#pragma once
// WASM shim: minimal thread.h replacing rpi-rgb-led-matrix thread.h
// Threading is managed by the JavaScript runtime in WASM builds.

namespace rgb_matrix {

// Stub Thread base class - not used in WASM scene rendering
class Thread {
public:
    virtual ~Thread() = default;
    virtual void Run() {}
    void Start() {}
    void WaitStopped() {}
};

} // namespace rgb_matrix

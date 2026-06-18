#pragma once

// =============================================================================
// tcxQTMRT - TrussC addon for the Qualisys QTM Real-Time (RT) protocol
// -----------------------------------------------------------------------------
// Streams 6DOF rigid bodies and 3D markers from Qualisys Track Manager (QTM)
// over the QTM RT protocol, wrapping the official qualisys_cpp_sdk (vendored
// and built from source).
//
//     QTMRT qtm;
//     qtm.setup("192.168.0.1");   // QTM host (base RT port 22222)
//     qtm.update();               // call once per frame (non-blocking)
//     for (size_t i = 0; i < qtm.getNumRigidBody(); i++) { ... }
//
// A background thread owns the connection, streams data, and fills a back
// buffer; update() swaps it to the front so the main thread reads lock-free.
// Positions are in millimetres (QTM native); use setScale()/setTransform().
// =============================================================================

// Pull in only the math types (not the full <TrussC.h>) so the addon TU stays
// light and avoids instantiating App/event template machinery.
#include <tcMath.h>

#include <memory>
#include <string>
#include <vector>

namespace tc = trussc;

namespace tcx {

class QTMRT {
public:
    struct RigidBody {
        int id = -1;
        std::string name;
        tc::Mat4 matrix;                 // full transform (scale/transform applied)
        tc::Vec3 position;               // convenience: matrix translation
        bool active = false;             // false when QTM reports the body as untracked

        bool isActive() const { return active; }
        const tc::Mat4& getMatrix() const { return matrix; }
        tc::Vec3 getPosition() const { return position; }
    };

    QTMRT();
    ~QTMRT();
    QTMRT(const QTMRT&) = delete;
    QTMRT& operator=(const QTMRT&) = delete;

    // Start the background receive thread. Returns immediately; connection
    // happens asynchronously and retries until close() / destruction.
    bool setup(const std::string& serverIp, unsigned short basePort = 22222);
    void update();                       // swap buffers; call once per frame
    void close();                        // stop thread + disconnect

    bool isConnected() const;
    int  getFrameNumber() const;
    float getDataRate() const;           // received frames/sec (approx)

    void setScale(float s);              // uniform scale on all positions
    void setTransform(const tc::Mat4& m);// arbitrary transform on all positions
    tc::Mat4 getTransform() const;

    // ---- 6DOF rigid bodies ----
    size_t getNumRigidBody() const;
    const RigidBody& getRigidBodyAt(size_t index) const;
    bool getRigidBody(int id, RigidBody& out) const;
    bool getRigidBodyByName(const std::string& name, RigidBody& out) const;

    // ---- 3D markers ----
    size_t getNumMarker() const;                  // labeled (Get3DMarker)
    const tc::Vec3& getMarker(size_t index) const;
    size_t getNumUnlabeledMarker() const;         // Get3DNoLabelsMarker
    const tc::Vec3& getUnlabeledMarker(size_t index) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace tcx

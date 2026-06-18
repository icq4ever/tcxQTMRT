#include "tcxQTMRT.h"

#include <tc/utils/tcLog.h>   // logNotice/logWarning (without full <TrussC.h>)

#include "RTProtocol.h"
#include "RTPacket.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <map>
#include <mutex>
#include <thread>

namespace tcx {

using tc::Vec3;
using tc::Mat4;

// ---------------------------------------------------------------------------

struct QTMRT::Impl {
    struct Frame {
        int frameNumber = 0;
        std::vector<RigidBody> rigidbodies;
        std::vector<Vec3> markers;
        std::vector<Vec3> unlabeled;
    };

    CRTProtocol rtProtocol;
    std::string serverIp;
    unsigned short basePort = 22222;

    std::thread thread;
    std::atomic<bool> running{false};
    std::atomic<bool> connected{false};

    std::mutex mutex;
    Frame back;                       // written by thread
    Frame front;                      // read by main thread

    std::map<int, int> idToIndex;
    std::map<std::string, int> nameToIndex;

    int frameNumber = 0;
    Mat4 transform = Mat4::identity();

    // data-rate estimate
    float dataRate = 0.0f;
    std::chrono::steady_clock::time_point lastTime{};
    int lastFrameNumber = 0;
    bool haveLastTime = false;

    void threadFunc();
    Mat4 buildMatrix(float x, float y, float z, const float rot[9], bool& active) const;
};

// Build a rigid-body transform from QTM position (mm) + 3x3 rotation.
//
// QTM's Get6DOFBody fills afRotMatrix[9] column-major (R[row][col]=rot[col*3+row]).
// TrussC's Mat4 is column-vector convention with row-major storage
// (at(row,col)=m[row*4+col]), so the rotation maps directly with the transpose
// of the array layout, i.e. row r / col c = rot[c*3 + r].
//
// Scale/transform is applied to the *position* only (rotation basis stays unit),
// matching ofxNatNet semantics. (Validate handedness against live QTM data; if a
// body's axes look mirrored, transpose the 3x3 below.)
Mat4 QTMRT::Impl::buildMatrix(float x, float y, float z, const float rot[9], bool& active) const {
    active = std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    if (!active) return Mat4::identity();

    Vec3 pos = transform * Vec3(x, y, z);

    Mat4 rotM(
        rot[0], rot[3], rot[6], 0.0f,
        rot[1], rot[4], rot[7], 0.0f,
        rot[2], rot[5], rot[8], 0.0f,
        0.0f,   0.0f,   0.0f,   1.0f);

    return Mat4::translate(pos) * rotM;
}

void QTMRT::Impl::threadFunc() {
    unsigned short udpPort = 0;

    while (running.load()) {
        if (!rtProtocol.Connected()) {
            connected = false;
            udpPort = 0;
            if (!rtProtocol.Connect(serverIp.c_str(), basePort, &udpPort)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }

            bool dataAvailable = false;
            rtProtocol.Read6DOFSettings(dataAvailable);  // load body names

            if (!rtProtocol.StreamFrames(
                    CRTProtocol::EStreamRate::RateAllFrames, 0, udpPort, nullptr,
                    CRTProtocol::cComponent6d | CRTProtocol::cComponent3d |
                        CRTProtocol::cComponent3dNoLabels)) {
                rtProtocol.Disconnect();
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }
            connected = true;
            tc::logNotice("tcxQTMRT") << "connected to QTM " << serverIp;
        }

        CRTPacket::EPacketType packetType;
        CNetwork::ResponseType res = rtProtocol.Receive(packetType, true, 1000000);

        if (res == CNetwork::ResponseType::disconnect ||
            res == CNetwork::ResponseType::error) {
            rtProtocol.Disconnect();
            connected = false;
            tc::logWarning("tcxQTMRT") << "connection lost; retrying";
            continue;
        }
        if (res != CNetwork::ResponseType::success) continue;   // timeout
        if (packetType != CRTPacket::PacketData) continue;

        CRTPacket* pkt = rtProtocol.GetRTPacket();
        if (!pkt) continue;

        Frame f;
        f.frameNumber = (int)pkt->GetFrameNumber();

        unsigned int bodyCount = pkt->Get6DOFBodyCount();
        f.rigidbodies.reserve(bodyCount);
        for (unsigned int i = 0; i < bodyCount; i++) {
            float x, y, z, rot[9];
            if (!pkt->Get6DOFBody(i, x, y, z, rot)) continue;

            RigidBody rb;
            rb.id = (int)i;
            const char* nm = rtProtocol.Get6DOFBodyName(i);
            rb.name = nm ? nm : ("body_" + std::to_string(i));
            rb.matrix = buildMatrix(x, y, z, rot, rb.active);
            rb.position = Vec3(rb.matrix.m[3], rb.matrix.m[7], rb.matrix.m[11]);
            f.rigidbodies.push_back(std::move(rb));
        }

        unsigned int markerCount = pkt->Get3DMarkerCount();
        f.markers.reserve(markerCount);
        for (unsigned int i = 0; i < markerCount; i++) {
            float x, y, z;
            if (!pkt->Get3DMarker(i, x, y, z)) continue;
            if (!std::isfinite(x)) continue;
            f.markers.push_back(transform * Vec3(x, y, z));
        }

        unsigned int nolabelCount = pkt->Get3DNoLabelsMarkerCount();
        f.unlabeled.reserve(nolabelCount);
        for (unsigned int i = 0; i < nolabelCount; i++) {
            float x, y, z;
            unsigned int id;
            if (!pkt->Get3DNoLabelsMarker(i, x, y, z, id)) continue;
            if (!std::isfinite(x)) continue;
            f.unlabeled.push_back(transform * Vec3(x, y, z));
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            back = std::move(f);
        }
    }
}

// ---------------------------------------------------------------------------

QTMRT::QTMRT() : impl(std::make_unique<Impl>()) {}

QTMRT::~QTMRT() { close(); }

bool QTMRT::setup(const std::string& serverIp, unsigned short basePort) {
    close();
    impl->serverIp = serverIp;
    impl->basePort = basePort;
    impl->running = true;
    impl->thread = std::thread(&Impl::threadFunc, impl.get());
    return true;
}

void QTMRT::close() {
    impl->running = false;
    if (impl->thread.joinable()) impl->thread.join();
    if (impl->rtProtocol.Connected()) {
        impl->rtProtocol.StreamFramesStop();
        impl->rtProtocol.Disconnect();
    }
    impl->connected = false;
    {
        std::lock_guard<std::mutex> lock(impl->mutex);
        impl->back = Impl::Frame();
    }
    impl->front = Impl::Frame();
    impl->idToIndex.clear();
    impl->nameToIndex.clear();
    impl->haveLastTime = false;
}

void QTMRT::update() {
    {
        std::lock_guard<std::mutex> lock(impl->mutex);
        impl->front = impl->back;
    }
    impl->frameNumber = impl->front.frameNumber;

    impl->idToIndex.clear();
    impl->nameToIndex.clear();
    for (int i = 0; i < (int)impl->front.rigidbodies.size(); i++) {
        const RigidBody& rb = impl->front.rigidbodies[i];
        impl->idToIndex[rb.id] = i;
        impl->nameToIndex[rb.name] = i;
    }

    auto now = std::chrono::steady_clock::now();
    if (impl->haveLastTime) {
        float dt = std::chrono::duration<float>(now - impl->lastTime).count();
        int df = impl->frameNumber - impl->lastFrameNumber;
        if (dt > 0.0f && df > 0)
            impl->dataRate = impl->dataRate * 0.9f + (df / dt) * 0.1f;
    }
    impl->lastTime = now;
    impl->lastFrameNumber = impl->frameNumber;
    impl->haveLastTime = true;
}

bool QTMRT::isConnected() const { return impl->connected.load(); }
int QTMRT::getFrameNumber() const { return impl->frameNumber; }
float QTMRT::getDataRate() const { return impl->dataRate; }

void QTMRT::setScale(float s) { impl->transform = Mat4::scale(s); }
void QTMRT::setTransform(const Mat4& m) { impl->transform = m; }
Mat4 QTMRT::getTransform() const { return impl->transform; }

size_t QTMRT::getNumRigidBody() const { return impl->front.rigidbodies.size(); }
const QTMRT::RigidBody& QTMRT::getRigidBodyAt(size_t index) const {
    return impl->front.rigidbodies[index];
}
bool QTMRT::getRigidBody(int id, RigidBody& out) const {
    auto it = impl->idToIndex.find(id);
    if (it == impl->idToIndex.end()) return false;
    out = impl->front.rigidbodies[it->second];
    return true;
}
bool QTMRT::getRigidBodyByName(const std::string& name, RigidBody& out) const {
    auto it = impl->nameToIndex.find(name);
    if (it == impl->nameToIndex.end()) return false;
    out = impl->front.rigidbodies[it->second];
    return true;
}

size_t QTMRT::getNumMarker() const { return impl->front.markers.size(); }
const Vec3& QTMRT::getMarker(size_t index) const { return impl->front.markers[index]; }
size_t QTMRT::getNumUnlabeledMarker() const { return impl->front.unlabeled.size(); }
const Vec3& QTMRT::getUnlabeledMarker(size_t index) const {
    return impl->front.unlabeled[index];
}

} // namespace tcx

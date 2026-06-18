#include "tcApp.h"
#include "mocapOscBridge.h"

void tcApp::setup() {
    setWindowTitle("tcxQTMRT - qtmrt2osc bridge");

    qtm.setup("127.0.0.1");       // QTM server ip
    osc.setup(oscHost, oscPort);  // OSC destination
}

void tcApp::update() {
    qtm.update();

    // forward only when a new QTM frame arrived
    int frame = qtm.getFrameNumber();
    if (frame == lastSentFrame) return;
    lastSentFrame = frame;

    for (size_t i = 0; i < qtm.getNumRigidBody(); i++) {
        const auto& rb = qtm.getRigidBodyAt(i);
        if (!rb.isActive() || rb.name.empty()) continue;

        // Full tcxFSportsMocap property set. QTM is typically Z-up already,
        // so yUpToZup stays false; flip it if your QTM streams Y-up.
        mocapOscBridge::send(osc, rb.name, rb.getMatrix(),
                             /*forward*/ tc::Vec3(0, 0, 1),
                             /*yUpToZup*/ false);
        sentMessages++;
    }
}

void tcApp::draw() {
    clear(0.07f, 0.07f, 0.09f);

    string info;
    info += "QTM -> OSC bridge (tcxFSportsMocap format)\n\n";
    info += string("QTM connected: ") + (qtm.isConnected() ? "YES" : "NO") + "\n";
    info += "QTM frame: " + to_string(qtm.getFrameNumber()) + "\n";
    info += "rigid bodies: " + to_string(qtm.getNumRigidBody()) + "\n\n";
    info += "OSC dest: " + oscHost + ":" + to_string(oscPort) + "\n";
    info += "  /rigidbody/<name>/{location3d,orientation,eye3d,...}\n";
    info += "bodies forwarded: " + to_string(sentMessages) + "\n";

    setColor(1.0f);
    drawBitmapString(info, 20, 20);
}

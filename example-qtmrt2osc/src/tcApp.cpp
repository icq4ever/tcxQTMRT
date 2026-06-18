#include "tcApp.h"
#include "mocapOscBridge.h"

void tcApp::setup() {
    setWindowTitle("tcxQTMRT - qtmrt2osc bridge");

    imguiSetup();

    connectQtm();   // QTM server
    applyOsc();     // OSC destination
}

void tcApp::connectQtm() {
    qtm.close();
    qtm.setup(qtmHost, (unsigned short)qtmPort);
}

void tcApp::applyOsc() {
    osc.setup(oscHost, oscPort);
    lastSentFrame = -1;   // force a resend on the next frame
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

    imguiBegin();

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("QTM -> OSC bridge");

    // ---- QTM source ----
    ImGui::SeparatorText("QTM server");
    ImGui::InputText("host##qtm", qtmHost, sizeof(qtmHost));
    ImGui::InputInt("RT base port", &qtmPort);
    if (ImGui::Button("Connect / Reconnect")) {
        connectQtm();
    }
    ImGui::SameLine();
    bool connected = qtm.isConnected();
    ImGui::TextColored(connected ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f)
                                 : ImVec4(0.9f, 0.4f, 0.4f, 1.0f),
                       connected ? "connected" : "disconnected");

    ImGui::Spacing();

    // ---- OSC destination ----
    ImGui::SeparatorText("OSC destination");
    ImGui::InputText("host##osc", oscHost, sizeof(oscHost));
    ImGui::InputInt("port", &oscPort);
    if (ImGui::Button("Apply OSC dest")) {
        applyOsc();
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Status");
    ImGui::Text("QTM frame: %d", qtm.getFrameNumber());
    ImGui::Text("rigid bodies: %d", (int)qtm.getNumRigidBody());
    ImGui::Text("bodies forwarded: %d", sentMessages);
    ImGui::TextDisabled("format: tcxFSportsMocap");
    ImGui::TextDisabled("/rigidbody/<name>/{location3d,orientation,...}");

    ImGui::End();

    imguiEnd();
}

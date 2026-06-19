#include "tcApp.h"
#include "mocapOscBridge.h"

void tcApp::setup() {
    setWindowTitle("tcxQTMRT - qtmrt2osc bridge");

    imguiSetup();

    // QTM streams millimetres; scale the camera rather than the data.
    cam.setDistance(2500);
    cam.setTarget(0, 0, 0);
    cam.setElevation(20);
    cam.enableMouseInput();

    loadSettings(); // restore last-used host/port before connecting

    connectQtm();   // QTM server
    applyOsc();     // OSC destination
}

void tcApp::loadSettings() {
    if (!fileExists(kSettingsPath)) return;

    Json j = loadJson(kSettingsPath);
    if (j.is_null()) return;

    string qh = j.value("qtmHost", string(qtmHost));
    string oh = j.value("oscHost", string(oscHost));
    snprintf(qtmHost, sizeof(qtmHost), "%s", qh.c_str());
    snprintf(oscHost, sizeof(oscHost), "%s", oh.c_str());
    qtmPort = j.value("qtmPort", qtmPort);
    oscPort = j.value("oscPort", oscPort);
}

void tcApp::saveSettings() {
    Json j;
    j["qtmHost"] = qtmHost;
    j["qtmPort"] = qtmPort;
    j["oscHost"] = oscHost;
    j["oscPort"] = oscPort;
    saveJson(j, kSettingsPath);
}

void tcApp::connectQtm() {
    qtm.close();
    qtm.setup(qtmHost, (unsigned short)qtmPort);
    saveSettings();   // persist the host/port we just applied
}

void tcApp::applyOsc() {
    osc.setup(oscHost, oscPort);
    lastSentFrame = -1;   // force a resend on the next frame
    saveSettings();   // persist the host/port we just applied
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

void tcApp::drawScene() {
    cam.begin();

    // Labels are screen-space, so collect them here (camera still active for
    // worldToScreen) and draw after cam.end(). Each carries its own colour.
    struct Label { Vec3 screen; string text; float r, g, b; };
    vector<Label> labels;

    // world origin axes — colour-coded X=red, Y=green, Z=blue, with end labels
    const float axisLen = 500.0f;
    setColor(1.0f, 0.2f, 0.2f); drawLine(Vec3(0, 0, 0), Vec3(axisLen, 0, 0));  // X
    setColor(0.2f, 1.0f, 0.2f); drawLine(Vec3(0, 0, 0), Vec3(0, axisLen, 0));  // Y
    setColor(0.2f, 0.4f, 1.0f); drawLine(Vec3(0, 0, 0), Vec3(0, 0, axisLen));  // Z
    labels.push_back({worldToScreen(Vec3(axisLen, 0, 0)), "X", 1.0f, 0.3f, 0.3f});
    labels.push_back({worldToScreen(Vec3(0, axisLen, 0)), "Y", 0.3f, 1.0f, 0.3f});
    labels.push_back({worldToScreen(Vec3(0, 0, axisLen)), "Z", 0.4f, 0.6f, 1.0f});

    for (size_t i = 0; i < qtm.getNumRigidBody(); i++) {
        const auto& rb = qtm.getRigidBodyAt(i);
        if (!rb.isActive()) continue;

        const Mat4& M = rb.getMatrix();
        Vec3 o(M.m[3], M.m[7], M.m[11]);
        Vec3 ax(M.m[0], M.m[4], M.m[8]);
        Vec3 ay(M.m[1], M.m[5], M.m[9]);
        Vec3 az(M.m[2], M.m[6], M.m[10]);
        float len = 120.0f;

        setColor(1.0f, 0.2f, 0.2f); drawLine(o, o + ax * len);  // X
        setColor(0.2f, 1.0f, 0.2f); drawLine(o, o + ay * len);  // Y
        setColor(0.2f, 0.4f, 1.0f); drawLine(o, o + az * len);  // Z

        Vec3 s = worldToScreen(o);
        if (s.z > 0.0f && s.z < 1.0f) {
            string name = rb.name.empty() ? ("rb " + to_string(i)) : rb.name;
            labels.push_back({s, name, 1.0f, 0.9f, 0.4f});
        }
    }

    cam.end();

    // labels (2D, drawn after the camera so they face the screen);
    // colour-coded per entry: axis end-caps X/Y/Z and rigid-body names
    for (const auto& l : labels) {
        if (l.screen.z <= 0.0f || l.screen.z >= 1.0f) continue;  // behind camera
        setColor(l.r, l.g, l.b);
        drawBitmapString(l.text, l.screen.x + 6, l.screen.y - 6);
    }
}

void tcApp::draw() {
    clear(0.07f, 0.07f, 0.09f);

    drawScene();
    drawGUI();
}

void tcApp::drawGUI() {
    imguiBegin();

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("QTM -> OSC bridge");

    // ---- QTM source ----
    // Edited host/port are persisted when applied via the buttons below
    // (connectQtm / applyOsc call saveSettings).
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

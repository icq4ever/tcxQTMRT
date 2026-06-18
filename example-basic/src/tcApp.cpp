#include "tcApp.h"

void tcApp::setup() {
    setWindowTitle("tcxQTMRT - basic");

    // QTM server ip (base RT port 22222). QTM itself runs on Windows;
    // this client connects over the network from any platform.
    qtm.setup("127.0.0.1");

    // QTM streams millimetres. Keep mm here; scale the camera instead.
    cam.setDistance(2500);
    cam.setTarget(0, 0, 0);
    cam.setElevation(20);
    cam.enableMouseInput();
}

void tcApp::update() {
    qtm.update();
}

void tcApp::draw() {
    clear(0.07f, 0.07f, 0.09f);

    cam.begin();

    // ground axes
    setColor(0.4f);
    drawLine(Vec3(0, 0, 0), Vec3(500, 0, 0));
    drawLine(Vec3(0, 0, 0), Vec3(0, 500, 0));
    drawLine(Vec3(0, 0, 0), Vec3(0, 0, 500));

    // labeled markers
    setColor(1.0f, 1.0f, 1.0f);
    for (size_t i = 0; i < qtm.getNumMarker(); i++)
        drawBox(qtm.getMarker(i), 15);

    // unlabeled markers
    setColor(1.0f, 1.0f, 1.0f, 0.35f);
    for (size_t i = 0; i < qtm.getNumUnlabeledMarker(); i++)
        drawBox(qtm.getUnlabeledMarker(i), 15);

    // 6DOF rigid bodies — draw local axes from the matrix basis columns.
    // Collect name labels in screen space (worldToScreen needs the camera
    // matrices, which cam.end() resets) and draw them in the 2D overlay below.
    struct Label { Vec3 screen; string text; };
    vector<Label> labels;

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

        // project the origin to screen for a floating label (skip if behind camera)
        Vec3 s = worldToScreen(o);
        if (s.z > 0.0f && s.z < 1.0f) {
            string name = rb.name.empty() ? ("rb " + to_string(i)) : rb.name;
            labels.push_back({s, name});
        }
    }

    cam.end();

    // rigid-body name labels (2D, drawn after the camera so they face the screen)
    setColor(1.0f, 0.9f, 0.4f);
    for (const auto& l : labels)
        drawBitmapString(l.text, l.screen.x + 6, l.screen.y - 6);

    // 2D overlay
    string info;
    info += string("connected: ") + (qtm.isConnected() ? "YES" : "NO") + "\n";
    info += "frame: " + to_string(qtm.getFrameNumber()) + "\n";
    info += "rigid bodies: " + to_string(qtm.getNumRigidBody()) + "\n";
    info += "labeled markers: " + to_string(qtm.getNumMarker()) + "\n";
    info += "unlabeled markers: " + to_string(qtm.getNumUnlabeledMarker()) + "\n";

    setColor(1.0f);
    drawBitmapString(info, 20, 20);
}

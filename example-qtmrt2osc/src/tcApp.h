#pragma once

#include <TrussC.h>
#include "tcxQTMRT.h"
#include "tcxOsc.h"
#include "tcxImGui.h"

using namespace std;
using namespace tc;
using namespace tcx;

// Bridge: receive 6DOF rigid bodies from QTM and forward them as OSC.
// Per rigid body, sends one message:
//   /qtm/rigidbody  <id:int> <name:string> <x> <y> <z> <m0..m8 rotation:float>
// This feeds OSC-based mocap consumers (e.g. tcxFSportsMocap, TouchDesigner).
//
// QTM host and OSC destination are editable at runtime via an ImGui panel.
class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;

    tcx::QTMRT qtm;
    tcx::OscSender osc;
    EasyCam cam;

    void drawScene();    // 3D rigid-body gizmos + name labels
    void drawGUI();

    void connectQtm();   // (re)start the QTM connection from qtmHost/qtmPort
    void applyOsc();     // (re)point the OSC sender at oscHost/oscPort

    // Editable settings (ImGui buffers).
    char qtmHost[64] = "127.0.0.1";
    int  qtmPort = 22222;          // QTM RT base port
    char oscHost[64] = "127.0.0.1";
    int  oscPort = 9000;

    int lastSentFrame = -1;
    int sentMessages = 0;
};

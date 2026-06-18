#pragma once

#include <TrussC.h>
#include "tcxQTMRT.h"
#include "tcxOsc.h"

using namespace std;
using namespace tc;

// Bridge: receive 6DOF rigid bodies from QTM and forward them as OSC.
// Per rigid body, sends one message:
//   /qtm/rigidbody  <id:int> <name:string> <x> <y> <z> <m0..m8 rotation:float>
// This feeds OSC-based mocap consumers (e.g. tcxFSportsMocap, TouchDesigner).
class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;

    tcx::QTMRT qtm;
    tcx::OscSender osc;

    string oscHost = "127.0.0.1";
    int oscPort = 9000;
    int lastSentFrame = -1;
    int sentMessages = 0;
};

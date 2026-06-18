#pragma once

#include <TrussC.h>
#include "tcxQTMRT.h"

using namespace std;
using namespace tc;

class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;

    tcx::QTMRT qtm;
    EasyCam cam;
};

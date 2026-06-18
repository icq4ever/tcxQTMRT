#include "TrussC.h"
#include "tcApp.h"

int main() {
    WindowSettings settings;
    settings.title = "tcxQTMRT - basic";
    settings.width = 1024;
    settings.height = 768;
    return TC_RUN_APP(tcApp, settings);
}

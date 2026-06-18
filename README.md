# tcxQTMRT

TrussC addon for receiving motion-capture data from **Qualisys Track Manager (QTM)** over the **QTM Real-Time (RT) protocol**.

Wraps the official [`qualisys_cpp_sdk`](https://github.com/qualisys/qualisys_cpp_sdk) (vendored under `qualisys_cpp_sdk/`, built from source) behind a small `tcx::QTMRT` class.

## Architecture

QTM is the **Windows-only server** that drives the cameras and tracks. This addon is the **client** — it connects over the network and receives the stream, and runs on Linux / macOS / Windows. (Same model as OptiTrack Motive + NatNet: server on Windows, client anywhere.)

## Features

- ✅ 6DOF rigid bodies (`tc::Mat4` transform + position + active flag)
- ✅ 3D labeled markers
- ✅ 3D unlabeled markers
- ✅ Background receive thread, double-buffered lock-free reads on the main thread
- ✅ Automatic reconnect
- ⏳ Skeletons / analog / force plates — not yet exposed

> **Rotation note:** QTM delivers 6DOF rotation as a column-major 3×3 matrix; it is mapped into TrussC's column-vector `Mat4`. Not yet validated on live hardware — if a body's axes look mirrored, transpose the 3×3 in `buildMatrix()` (see `src/tcxQTMRT.cpp`).

## Usage

```cpp
#include "tcxQTMRT.h"

tcx::QTMRT qtm;

void tcApp::setup() {
    qtm.setup("192.168.0.1");   // QTM host (base RT port 22222)
}

void tcApp::update() {
    qtm.update();               // non-blocking; call once per frame
}

void tcApp::draw() {
    for (size_t i = 0; i < qtm.getNumRigidBody(); i++) {
        const auto& rb = qtm.getRigidBodyAt(i);
        // rb.getMatrix(), rb.getPosition(), rb.name, rb.isActive()
    }
    for (size_t i = 0; i < qtm.getNumMarker(); i++)
        drawBox(qtm.getMarker(i), 15);
}
```

Positions are in **millimetres** (QTM native). Use `setScale(s)` or `setTransform(Mat4)` to convert; scale is applied to positions only, not to body orientation.

## Examples

- **example-basic** — 3D viewer: markers as boxes, rigid bodies as axis gizmos.
- **example-qtmrt2osc** — bridge that forwards QTM rigid bodies as OSC (`/qtm/rigidbody …`), for OSC-based consumers like `tcxFSportsMocap` or TouchDesigner. Depends on `tcxOsc`.

## Build

The Qualisys SDK builds from source via `add_subdirectory`, so there are no prebuilt binaries to manage.

- **Linux / macOS**: no extra dependencies.
- **Windows**: `ws2_32` + `iphlpapi` (propagated automatically from the SDK target).

Add `tcxQTMRT` to your project's `addons.make` and run `trusscli update`.

## License

MIT (addon glue). The bundled `qualisys_cpp_sdk` keeps its own license — see `LICENSE.qualisys_cpp_sdk.md`.

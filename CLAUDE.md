# tcxQTMRT — context for Claude Code

TrussC addon wrapping the **Qualisys QTM Real-Time (RT) protocol** (vendored
`qualisys_cpp_sdk`, built from source). Sibling project: an openFrameworks
version `ofxQTMRT` lives at `~/gits/works/qualisys/ofxQTMRT` (same idea, oF types).

Full origin conversation (design decisions + build) is in `docs/SESSION-LOG.md`
(readable) and `docs/session-transcript.jsonl` (verbatim).

## What this is

- `tcx::QTMRT` (src/tcxQTMRT.{h,cpp}): connects to a QTM server, streams **6DOF
  rigid bodies + 3D markers (labeled & unlabeled)**, exposes them as `tc::Mat4` /
  `tc::Vec3`. API modelled after ofxNatNet: `setup()` / `update()` / getters.
- Background thread owns the connection + `Receive` loop, fills a back buffer;
  `update()` swaps to a front buffer so the main thread reads lock-free. Auto-reconnect.
- Positions in **mm** (QTM native); `setScale()` / `setTransform()` affect positions only.

## Architecture note (why client is cross-platform)

QTM is the **Windows-only server** (drives cameras, tracks). This addon is the
**client** — runs on Linux/macOS/Windows, connects over the network. Identical
model to OptiTrack: Motive(server, Windows) + NatNet(protocol) ≈ QTM + QTM RT.

## Key decisions (already settled — don't relitigate)

- **Name**: `tcxQTMRT`. Chose the protocol name (QTM RT) over vendor `tcxQualisys`,
  mirroring ofxNatNet and Qualisys's own Python pkg rename `qtm` → `qtm_rt`.
- **SDK vendoring**: full SDK copied to `qualisys_cpp_sdk/` + `add_subdirectory`
  (same pattern as neighbor `tcxNozzle/nozzle/`). Examples/tests/VS-solution files
  were trimmed. Windows libs `ws2_32`/`iphlpapi` propagate from the SDK target.
- **OSC sending**: kept OUT of the core class (single responsibility). Provided as
  a separate **example-qtmrt2osc** bridge combining `tcxQTMRT` + `tcxOsc`.

## Gotchas already solved (keep them solved)

- **Do NOT `#include <TrussC.h>` in the addon** — it instantiates App/event/reflect
  template machinery and fails to compile in a library TU. Include only `<tcMath.h>`
  (types) and `<tc/utils/tcLog.h>` (logging), and alias `namespace tc = trussc;`.
- **C++20 required**: TrussC core headers use concepts/`requires`. CMakeLists sets
  `CMAKE_CXX_STANDARD 20` (SDK only needs 14; 20 satisfies both). C++17 fails.

## Open items / unverified (need live QTM hardware)

1. **Rotation mapping unvalidated.** QTM gives a column-major 3×3; mapped into
   TrussC's column-vector `Mat4` in `buildMatrix()`. If a body's axes look mirrored
   on real data, transpose the 3×3. (ofQTMRT uses the *opposite* mapping because oF
   is row-vector convention — expected.)
2. **Data path** (actual rigid bodies/markers) only verified to "connected: NO" —
   no QTM server available. example-basic renders correctly (axes + overlay).
3. **macOS/Windows builds** untested here; only Linux built + ran.
4. **Not yet exposed**: skeletons, analog, force plates (reachable via the SDK).

## Build & verify

```bash
cd example-basic && trusscli update && trusscli build   # or example-qtmrt2osc
# runtime check (Linux/macOS, needs display):
TRUSSC_MCP=1 TRUSSC_MCP_PORT=8092 ./bin/example-basic &
curl -s -X POST localhost:8092/mcp -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","method":"tools/call","id":2,"params":{"name":"save_screenshot","arguments":{"path":"/tmp/v.png"}}}'
```

After adding/renaming source files run `trusscli update`. Use TrussC conventions:
`tc::` types, 0–1 colors, `logNotice/logWarning` not cout, `drawBox/drawLine`.

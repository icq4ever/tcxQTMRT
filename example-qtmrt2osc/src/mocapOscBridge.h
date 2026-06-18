#pragma once

// =============================================================================
// mocapOscBridge - emit a rigid body as the tcxFSportsMocap OSC property set.
//   Address: /rigidbody/<name>/<property>
//   location3d, location2d, audioLocation2d, height, orientation(quat w,x,y,z),
//   direction(deg), eye3d, eye2d, pitch(deg), eulerAngles(deg), matrix(16f).
//
// Everything is derived from the rigid body's full transform (tc::Mat4, column-
// vector / row-major as in tcMath.h). Consumer convention is Z-UP floor
// (height = z, floor = xy). Two knobs you may need to match your setup:
//   - forward : which local axis points "forward" (eye/direction/pitch).
//               OptiTrack/Qualisys rigid bodies vary; default +Z.
//   - yUpToZup: remap a Y-up stream (Motive default) to the Z-up consumer.
//               Leave false if your mocap already streams Z-up (typical QTM).
// =============================================================================

#include <TrussC.h>
#include "tcxOsc.h"
#include <cmath>
#include <string>

namespace mocapOscBridge {

inline tc::Vec3 col(const tc::Mat4& M, int c) {              // basis column (world dir of local axis c)
    return tc::Vec3(M.m[0 + c], M.m[4 + c], M.m[8 + c]);
}

// Rotation matrix -> quaternion (w,x,y,z). R[r][c] = M.m[r*4 + c].
inline void matToQuat(const tc::Mat4& M, float& w, float& x, float& y, float& z) {
    float m00 = M.m[0],  m01 = M.m[1],  m02 = M.m[2];
    float m10 = M.m[4],  m11 = M.m[5],  m12 = M.m[6];
    float m20 = M.m[8],  m21 = M.m[9],  m22 = M.m[10];
    float tr = m00 + m11 + m22;
    if (tr > 0.0f) {
        float s = std::sqrt(tr + 1.0f) * 2.0f;
        w = 0.25f * s; x = (m21 - m12) / s; y = (m02 - m20) / s; z = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        w = (m21 - m12) / s; x = 0.25f * s; y = (m01 + m10) / s; z = (m02 + m20) / s;
    } else if (m11 > m22) {
        float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        w = (m02 - m20) / s; x = (m01 + m10) / s; y = 0.25f * s; z = (m12 + m21) / s;
    } else {
        float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        w = (m10 - m01) / s; x = (m02 + m20) / s; y = (m12 + m21) / s; z = 0.25f * s;
    }
}

inline void sendFloat1(tcx::OscSender& osc, const std::string& base, const char* prop, float a) {
    tcx::OscMessage m; m.setAddress(base + prop); m.addFloat(a); osc.send(m);
}
inline void sendFloat2(tcx::OscSender& osc, const std::string& base, const char* prop, float a, float b) {
    tcx::OscMessage m; m.setAddress(base + prop); m.addFloat(a); m.addFloat(b); osc.send(m);
}
inline void sendFloat3(tcx::OscSender& osc, const std::string& base, const char* prop, float a, float b, float c) {
    tcx::OscMessage m; m.setAddress(base + prop); m.addFloat(a); m.addFloat(b); m.addFloat(c); osc.send(m);
}

// Send the full property set for one rigid body.
//   M        : full transform (rotation in the upper-left 3x3, translation in m[3,7,11])
//   forward  : local forward axis (default +Z)
//   yUpToZup : remap Y-up -> Z-up before computing (default off)
inline void send(tcx::OscSender& osc, const std::string& name, const tc::Mat4& Min,
                 tc::Vec3 forward = tc::Vec3(0, 0, 1), bool yUpToZup = false) {
    const float RAD2DEG = 57.2957795f;
    const std::string base = "/rigidbody/" + name + "/";

    tc::Mat4 M = Min;
    if (yUpToZup) {
        // NatNet (Motive) streams Y-up. Convert to the Z-up consumer frame,
        // matching the original natnet2osc (ofApp: mat.rotate(180,0,1,1)):
        //   (x, y, z) -> (-x, z, y)   [new Z = old Y = vertical]
        // World re-basis only (left-multiply), local frame unchanged.
        tc::Mat4 C(-1,0,0,0,  0,0,1,0,  0,1,0,0,  0,0,0,1);
        M = C * Min;
    }

    tc::Vec3 pos(M.m[3], M.m[7], M.m[11]);

    // facing direction = rotation applied to the forward axis
    tc::Vec3 e3(
        M.m[0] * forward.x + M.m[1] * forward.y + M.m[2]  * forward.z,
        M.m[4] * forward.x + M.m[5] * forward.y + M.m[6]  * forward.z,
        M.m[8] * forward.x + M.m[9] * forward.y + M.m[10] * forward.z);
    float e3len = std::sqrt(e3.x * e3.x + e3.y * e3.y + e3.z * e3.z);
    if (e3len > 1e-6f) { e3.x /= e3len; e3.y /= e3len; e3.z /= e3len; }
    float floorLen = std::sqrt(e3.x * e3.x + e3.y * e3.y);
    tc::Vec2 e2(0, 0);
    if (floorLen > 1e-6f) { e2.x = e3.x / floorLen; e2.y = e3.y / floorLen; }

    float w, qx, qy, qz;
    matToQuat(M, w, qx, qy, qz);
    tc::Vec3 euler = tc::Quaternion(w, qx, qy, qz).toEuler();   // radians -> deg below

    sendFloat3(osc, base, "location3d", pos.x, pos.y, pos.z);
    sendFloat2(osc, base, "location2d", pos.x, pos.y);
    sendFloat2(osc, base, "audioLocation2d", pos.y, -pos.x);     // 90deg CW
    sendFloat1(osc, base, "height", pos.z);

    { tcx::OscMessage m; m.setAddress(base + "orientation");
      m.addFloat(w); m.addFloat(qx); m.addFloat(qy); m.addFloat(qz); osc.send(m); }

    // direction: matches the original natnet2osc (range 0..360)
    sendFloat1(osc, base, "direction", std::atan2(-e3.y, -e3.x) * RAD2DEG + 180.0f);
    sendFloat3(osc, base, "eye3d", e3.x, e3.y, e3.z);
    sendFloat2(osc, base, "eye2d", e2.x, e2.y);
    sendFloat1(osc, base, "pitch", std::atan2(e3.z, floorLen) * RAD2DEG);
    sendFloat3(osc, base, "eulerAngles", euler.x * RAD2DEG, euler.y * RAD2DEG, euler.z * RAD2DEG);

    { tcx::OscMessage m; m.setAddress(base + "matrix");
      for (int i = 0; i < 16; i++) m.addFloat(M.m[i]); osc.send(m); }
}

} // namespace mocapOscBridge

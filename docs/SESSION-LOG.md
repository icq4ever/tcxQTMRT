# tcxQTMRT — session transcript (rendered)

Readable rendering of the design+build conversation that created this addon (and its sibling ofxQTMRT). Verbatim original: `session-transcript.jsonl`.

Tool calls are shown as compact markers; full args/outputs are in the jsonl.


---


## 🧑 User

여기 ofxNatNet은 optitrack의 natnet 프로토콜을 oF에서 사용할수 있게 변환하는 프로그램이야.

qualisys_cpp_sdk는 qualisys의 sdk이고..

ofxQualisys로 만들수 있을까 ?


## 🤖 Assistant

네, 충분히 가능합니다. 먼저 실제 가지고 있는 파일들을 살펴보겠습니다.


## 🤖 Assistant

> 🔧 `Bash` — List project directory structure


## 🤖 Assistant

Let me explore both projects in parallel to understand their structures.


## 🤖 Assistant

> 🔧 `Bash` — Explore ofxNatNet structure


## 🤖 Assistant

> 🔧 `Bash` — Explore qualisys SDK structure


## 🤖 Assistant

Let me look at the key API surfaces — the ofxNatNet header to understand the addon's public API, and the Qualisys SDK's main protocol/packet headers plus the RigidBodyStreaming example (the closest analog).


## 🤖 Assistant

> 🔧 `Read` — /home/icq4ever/gits/works/qualisys/ofxNatNet/src/ofxNatNet.h


## 🤖 Assistant

> 🔧 `Read` — /home/icq4ever/gits/works/qualisys/qualisys_cpp_sdk/RigidBodyStreaming/RigidBodyStreaming.cpp


## 🤖 Assistant

Now let me look at the example ofApp usage and the relevant Qualisys protocol API for markers/skeletons (not just 6DOF).


## 🤖 Assistant

> 🔧 `Read` — /home/icq4ever/gits/works/qualisys/ofxNatNet/example/src/ofApp.cpp


## 🤖 Assistant

> 🔧 `Bash` — Find key Qualisys API methods


## 🤖 Assistant

네, 충분히 가능하고 **오히려 ofxNatNet보다 더 쉽습니다.** 두 코드를 다 살펴본 결과를 정리하면:

## 왜 더 쉬운가

ofxNatNet은 NatNet 프로토콜을 Poco 소켓으로 **직접 파싱**합니다 ([ofxNatNet.cpp](ofxNatNet/src/ofxNatNet.cpp)가 30KB나 되는 이유). 반면 qualisys_cpp_sdk는 이미 `CRTProtocol`이라는 고수준 클래스가 네트워킹 + 패킷 파싱을 다 해줍니다. [RigidBodyStreaming.cpp](qualisys_cpp_sdk/RigidBodyStreaming/RigidBodyStreaming.cpp)를 보면 연결→스트리밍 요청→수신이 몇 줄로 끝납니다.

즉 ofxQualisys는 **SDK를 oF 친화적으로 감싸는(wrap) 얇은 레이어**만 만들면 됩니다.

## 구조 매핑

ofxNatNet의 공개 API를 거의 그대로 흉내낼 수 있습니다:

| ofxNatNet | qualisys_cpp_sdk 대응 |
|---|---|
| `natnet.setup(iface, serverIp)` | `CRTProtocol::Connect(serverAddr, basePort, &udpPort)` |
| 내부 수신 스레드 | `Receive(packetType, true)` 를 도는 스레드 (ofThread) |
| `getRigidBodyAt(i)` / `RigidBody.matrix` | `Get6DOFBody(i, x,y,z, rotMatrix[9])` + `Get6DOFBodyName(i)` |
| `getMarker(i)` | `Get3DMarker(i, x,y,z)` / `Get3DMarkerCount()` |
| `getNumFilterdMarker` (unlabeled) | `Get3DNoLabelsMarker(...)` |
| `getSkeletonAt(j)` | `GetSkeletonCount()` / `GetSkeletonSegment(...)` |
| 컴포넌트 선택 | `StreamFrames(..., cComponent6d \| cComponent3d \| cComponentSkeleton)` |

## 주의할 핵심 차이점 3가지

1. **단위**: Qualisys는 기본 **밀리미터(mm)**. ofxNatNet의 `setScale()` 같은 변환 레이어를 그대로 두면 호환됩니다.
2. **회전 데이터**: Qualisys는 `float rotMatrix[9]` (3×3, column-major)로 줍니다. 이걸 `ofMatrix4x4`로 조립하는 헬퍼가 필요합니다.
3. **연결 방식**: NatNet은 multicast UDP, Qualisys는 TCP 명령포트(기본 22222) + UDP 데이터 스트림. `setup()` 인자가 조금 달라집니다 (multicast group 불필요).

## 빌드 고려사항

ofxNatNet은 Poco(oF 내장)만 쓰지만, qualisys_cpp_sdk는 별도 `.cpp` 파일들(`RTProtocol.cpp`, `RTPacket.cpp`, `Network.cpp`, `SettingsDeserializer.cpp` 등)을 같이 컴파일해야 합니다. 이걸 addon의 `libs/` 또는 `src/`에 포함시키면 됩니다.

---

원하시면 제가 바로 `ofxQualisys` addon 스캐폴드를 만들어드릴 수 있습니다. 만들기 전에 방향만 확인할게요:


## 🤖 Assistant

> 🔧 `AskUserQuestion`


## 🤖 Assistant

좋은 질문이에요. 두 스타일이 **실제 사용 코드에서 어떻게 달라지는지** 같은 작업(6DOF 바디 그리기)으로 비교해드릴게요.

## 스타일 A — ofxNatNet 호환

addon이 내부 스레드에서 데이터를 받아 oF 타입으로 **변환·캐싱**해두고, 사용자는 getter만 호출합니다.

```cpp
// ofApp.h
ofxQualisys qualisys;

// setup()
qualisys.setup("192.168.0.1");   // QTM 서버 IP만
qualisys.setScale(1.0);          // mm 단위 그대로

// update()
qualisys.update();               // 캐시 스왑만, 논블로킹

// draw()
for (int i = 0; i < qualisys.getNumRigidBody(); i++) {
    const auto& RB = qualisys.getRigidBodyAt(i);
    ofPushMatrix();
    glMultMatrixf(RB.getMatrix().getPtr());   // 이미 ofMatrix4x4로 조립됨
    ofDrawAxis(30);
    ofDrawBitmapString(RB.name, 0, 10);
    ofPopMatrix();
}
for (int i = 0; i < qualisys.getNumMarker(); i++)
    ofDrawBox(qualisys.getMarker(i), 3);     // 이미 ofVec3f
```

**특징**
- `Get6DOFBody`의 `float[9]` 회전행렬 → `ofMatrix4x4` 변환을 **addon이 대신** 해줌
- 수신 스레드 + 더블버퍼링을 addon이 관리 → `update()`는 메인스레드에서 안전
- 기존 ofxNatNet 예제 코드를 거의 그대로 복붙 가능 (메서드 이름이 같으니까)
- 단점: SDK의 모든 기능(세팅 읽기, 디스커버리, GUI 제어 등)을 다 노출하진 않음 — addon이 골라서 감쌈

## 스타일 B — Qualisys 네이티브

`CRTProtocol`을 거의 그대로 노출하고, oF 타입 헬퍼만 얹습니다.

```cpp
// ofApp.h
CRTProtocol rtProtocol;

// 사용자가 직접 SDK 흐름을 관리
unsigned short udpPort = 6734;
rtProtocol.Connect("192.168.0.1", 22222, &udpPort);
rtProtocol.StreamFrames(CRTProtocol::EStreamRate::RateAllFrames,
                        0, udpPort, NULL,
                        CRTProtocol::cComponent6d | CRTProtocol::cComponent3d);

// update()/draw() — 사용자가 직접 Receive 루프와 변환 처리
CRTPacket::EPacketType type;
if (rtProtocol.Receive(type, true) == CNetwork::ResponseType::success && type == CRTPacket::PacketData) {
    CRTPacket* pkt = rtProtocol.GetRTPacket();
    float x, y, z, rot[9];
    for (uint i = 0; i < pkt->Get6DOFBodyCount(); i++) {
        pkt->Get6DOFBody(i, x, y, z, rot);
        ofMatrix4x4 m = ofxQualisys::toMatrix(x, y, z, rot);  // 헬퍼만 제공
        // ... 직접 그리기
    }
}
```

**특징**
- SDK의 **100% 기능**에 접근 (세팅 읽기, 라벨/이름, 디스커버리, force plate, gaze 등 뭐든)
- 대신 스레딩·버퍼링·`Receive` 블로킹 처리를 **사용자가 직접** 신경 써야 함 (`Receive(..., true)`는 블로킹이라 draw 루프에서 그냥 쓰면 프레임 드랍)
- oF스럽지 않음 — SDK 문서를 보면서 써야 함

## 핵심 차이 한 줄 요약

| | 스타일 A (호환) | 스타일 B (네이티브) |
|---|---|---|
| 스레딩/버퍼링 | addon이 처리 | 사용자가 처리 |
| 회전→ofMatrix | 자동 | 헬퍼 호출 |
| 노출 기능 | addon이 선별 | SDK 전체 |
| 코드 이식성 | ofxNatNet 그대로 | QTM SDK 스타일 |
| 처음 쓰기 | 쉬움 | 자유롭지만 손이 감 |

---

## 제 추천

**스타일 A (ofxNatNet 호환)** 을 권합니다. 이유는:

1. oF 사용자 입장에선 "`update()` 부르고 getter로 꺼내 쓴다"가 직관적이고, `Receive`의 블로킹/스레딩 함정을 피할 수 있습니다.
2. 이미 ofxNatNet으로 짠 프로젝트가 있다면 **모캡 시스템만 바꿔 끼우기**가 쉬워집니다.
3. SDK 고급 기능이 나중에 필요해지면, addon이 내부에 들고 있는 `CRTProtocol&`를 `getProtocol()`로 노출해주면 됩니다 — **두 스타일의 장점을 다 가질 수 있어요** (A를 기본으로, 필요시 B로 탈출).

질문: 혹시 지금 만들려는 게 **기존 ofxNatNet 기반 프로젝트를 Qualisys로 이식**하는 건가요, 아니면 **새 프로젝트**인가요? 이게 스타일 결정의 가장 큰 변수라서요. 그리고 빌드 타겟은 어디인가요(Linux/macOS/Windows)?


## 🧑 User

A로 하자. 빌드 타겟은 linux/macOS/windows 다


## 🤖 Assistant

A로 결정. 크로스플랫폼 빌드를 위해 SDK가 정확히 어떤 소스/의존성을 요구하는지 먼저 확인할게요.


## 🤖 Assistant

> 🔧 `Read` — /home/icq4ever/gits/works/qualisys/qualisys_cpp_sdk/CMakeLists.txt


## 🤖 Assistant

> 🔧 `Bash` — Inspect SDK deps and platform code


## 🤖 Assistant

빌드 의존성 확인 완료. **크로스플랫폼에 아주 유리한 조건**이에요:

- 외부 라이브러리 의존성이 **tinyxml2 하나뿐인데 그게 SDK 안에 번들**되어 있음 (`External/tinyxml2`)
- 추가 시스템 라이브러리는 **Windows만** `ws2_32`, `iphlpapi` 필요. Linux/macOS는 표준 BSD 소켓이라 **추가 링크 없음**
- C++14 요구 (oF 0.11+ 기본 충족)

즉 별도 prebuilt 바이너리 없이 **SDK를 소스째로 addon에 넣고 같이 컴파일**하면 3개 플랫폼 다 됩니다.

## 최종 설계

### 디렉토리 구조 (oF addon 규칙)
```
ofxQualisys/
├── addon_config.mk              # 플랫폼별 링크 플래그 (Windows만 ws2_32/iphlpapi)
├── src/
│   ├── ofxQualisys.h            # 공개 API (스타일 A)
│   └── ofxQualisys.cpp          # 수신 스레드 + ofMatrix 변환 + 더블버퍼
├── libs/
│   └── qualisys_cpp_sdk/
│       ├── include/             # *.h (RTProtocol.h 등) + tinyxml2.h
│       └── src/                 # *.cpp 9개 + tinyxml2.cpp
└── example/
    ├── addons.make              # "ofxQualisys"
    └── src/                     # ofApp.{h,cpp}, main.cpp
```
> oF projectGenerator가 `libs/*/include`는 헤더 경로로, `libs/*/src`는 소스로 자동 인식합니다.

### addon_config.mk 핵심
```makefile
common:
	ADDON_INCLUDES += libs/qualisys_cpp_sdk/include
	ADDON_INCLUDES += libs/qualisys_cpp_sdk/include/tinyxml2

vs:
	ADDON_LIBS += ws2_32
	ADDON_LIBS += iphlpapi
# linux64, osx: 추가 없음
```

### 공개 API (ofxNatNet 호환 + 내부 스레드)
```cpp
class ofxQualisys {
public:
    typedef ofVec3f Marker;
    class RigidBody { int id; string name; ofMatrix4x4 matrix; bool isActive(); ... };

    void setup(string serverIp, unsigned short basePort = 22222);
    void update();                       // 더블버퍼 스왑(논블로킹)
    bool isConnected() const;
    int  getFrameNumber() const;

    void setScale(float v);              // mm → 원하는 단위
    void setTransform(const ofMatrix4x4&);

    // 6DOF
    size_t getNumRigidBody() const;
    const RigidBody& getRigidBodyAt(int) const;
    bool getRigidBody(int id, RigidBody&) const;
    bool getRigidBodyByName(string, RigidBody&) const;

    // 3D markers (labeled + unlabeled)
    size_t getNumMarker() const;         // labeled (Get3DMarker)
    const Marker& getMarker(size_t) const;
    size_t getNumUnlabeledMarker() const;// Get3DNoLabelsMarker
    const Marker& getUnlabeledMarker(size_t) const;

    CRTProtocol& getProtocol();          // 고급기능 탈출구 (스타일 B로)

    void debugDraw();                    // ofxNatNet처럼 디버그 렌더
};
```

### 스레딩 모델 (핵심 설계 포인트)
- 내부 `ofThread` 하나가 `Connect` → `StreamFrames(... cComponent6d | cComponent3d ...)` → `Receive(type, true)` **블로킹 루프**를 돌림
- 받은 프레임을 mutex 보호 하에 "백버퍼"에 쌓고, `update()`에서 프론트버퍼로 스왑 → 메인스레드(draw)는 락 없이 읽기
- 연결 끊기면 스레드가 자동 재연결 시도 (RigidBodyStreaming 예제의 재연결 루프 패턴)
- 회전행렬 `float[9]`(column-major) → `ofMatrix4x4` 조립은 변환 시점에 한 번만

이 구조면 ofxNatNet 예제의 `ofApp.cpp`가 메서드 이름만 바꿔 거의 그대로 돌아갑니다.

---

설계는 여기까지면 구현에 충분합니다. 한 가지만 확인할게요:

**SDK 소스를 addon `libs/`에 복사해서 자체 컴파일**하는 방식으로 갈까요(추천 — 사용자가 addon만 받으면 끝, 크로스플랫폼 깔끔)? 아니면 **SDK를 미리 정적 라이브러리로 빌드해두고 링크**만 할까요(addon 컴파일은 빨라지지만 플랫폼별 `.a`/`.lib`를 따로 관리해야 함)?

이것만 정해지면 바로 만들겠습니다.


## 🧑 User

자체 컴파일로


## 🤖 Assistant

자체 컴파일로 결정. 만들겠습니다. 먼저 정확한 SDK 시그니처 몇 개를 확인하고 시작할게요.


## 🤖 Assistant

> 🔧 `Bash` — Confirm SDK method signatures


## 🤖 Assistant

> 🔧 `Bash` — Get Connect/StreamFrames/StreamRate signatures


## 🤖 Assistant

> 🔧 `Bash` — Get Connect/StreamFrames signatures


## 🤖 Assistant

> 🔧 `Bash` — Get signatures with absolute path


## 🤖 Assistant

> 🔧 `Bash` — Find Connect, EStreamRate enum, component constants


## 🤖 Assistant

> 🔧 `Read` — /home/icq4ever/gits/works/qualisys/qualisys_cpp_sdk/RTProtocol.h


## 🤖 Assistant

SDK 시그니처 모두 확인했습니다. 이제 디렉토리를 구성하고 SDK 소스를 복사하겠습니다.


## 🤖 Assistant

> 🔧 `Bash` — Check include styles


## 🤖 Assistant

좋은 지적이에요. 이름 짓기 전에 짚고 갈 부분입니다.

OptiTrack은 프로토콜 브랜드가 **"NatNet"** 으로 확실히 있어서 `ofxNatNet`이 된 거고요. Qualisys 쪽을 따져보면:

- Qualisys의 실시간 프로토콜 공식 명칭은 **"QTM Real-Time (RT) Protocol"** 입니다 (QTM = Qualisys Track Manager).
- 그래서 SDK 클래스들이 전부 `CRT...` (CRTProtocol, CRTPacket) 접두사를 씁니다.
- Qualisys 공식 SDK 네이밍을 보면: C++ = `qualisys_cpp_sdk`, **Python = `qualisys_python_sdk`인데 import 패키지명은 `qtm` / `qtm_rt`** 예요.

즉 NatNet처럼 딱 떨어지는 프로토콜 브랜드명은 없고, 생태계에서 통용되는 약칭은 **QTM** 또는 **RT Protocol**입니다.

정리하면 후보는:

| 이름 | 근거 | 비고 |
|---|---|---|
| **ofxQualisys** | 벤더명 | oF에선 벤더명 addon 흔함(ofxKinect, ofxRealSense). 검색·인지 제일 쉬움 |
| **ofxQTM** | 프로토콜/소프트웨어 약칭 | ofxNatNet의 "프로토콜로 이름짓기" 논리를 가장 잘 따름. Python SDK도 `qtm` |
| ofxQualisysRT / ofxQTMRT | 정식 프로토콜명 | 정확하지만 길고 덜 쓰임 |

**제 추천: `ofxQTM`** — "프로토콜명으로 짓는다"는 ofxNatNet의 논리를 그대로 따르고, Qualisys 공식 Python SDK도 `qtm`를 쓰니 일관됩니다. 다만 인지도/검색 편의를 최우선한다면 `ofxQualisys`도 충분히 정당합니다.

어느 걸로 갈까요? (아직 파일 생성 전이라 지금 정하면 깔끔합니다)


## 🤖 Assistant

> 🔧 `AskUserQuestion`


## 🤖 Assistant

비슷한 사례를 찾아보면 명확한 패턴이 있어요.

## 다른 생태계의 Qualisys 연동 네이밍

| 플랫폼 | 이름 | 기준 |
|---|---|---|
| **Qualisys Python SDK** | 패키지 `qtm` → 최신 버전에서 **`qtm_rt`로 개명** | 프로토콜(RT) 명시 |
| Qualisys Unity SDK | 내부 클래스 `RTProtocol`, `RTClient` | 프로토콜(RT) |
| Qualisys C++ SDK (지금 이거) | `RTClientSDK.sln`, `RTClientExample`, `CRT...` | 프로토콜(RT) |
| Unreal 플러그인 | "Qualisys" 타이틀 | 벤더명 |

## 모캡 addon들의 oF 관례

| addon | 기준 | 비고 |
|---|---|---|
| **ofxNatNet** | 프로토콜 브랜드 | NatNet이 OptiTrack의 강한 브랜드라 예외적 |
| **ofxVicon** | 벤더명 | 프로토콜명("DataStream") 안 씀 |
| **ofxPhaseSpace** | 벤더명 | — |

## 핵심 포인트 (QTM vs QTMRT 판가름)

**"QTM" 단독은 사실 프로토콜이 아니라 데스크톱 소프트웨어(Qualisys Track Manager) 이름**이에요. 우리가 감싸는 건 그 소프트웨어가 아니라 **실시간 프로토콜(RT)** 이고요. 그래서:

- Qualisys 공식 Python SDK가 일부러 `qtm` → **`qtm_rt`로 바꾼** 게 결정적 단서입니다. "그냥 QTM이 아니라 RT 프로토콜이다"를 분명히 하려고요.
- C++ SDK 자신도 전부 `RT` 접두사(`CRTProtocol`, `RTClientSDK`)를 씁니다.

## 결론

정확성·공식 정렬 기준이면 **`ofxQTMRT`** 가 맞습니다 (= Python의 `qtm_rt`와 동일한 논리, "QTM의 RT 프로토콜"). `ofxQTM`은 짧지만 엄밀히는 소프트웨어 이름이라 살짝 부정확해요.

다만 솔직하게 트레이드오프를 말하면 — `ofxQTMRT`는 처음 보는 사람이 "이게 뭐지" 싶을 수 있고, oF 모캡 addon 다수(ofxVicon/ofxPhaseSpace)는 그냥 벤더명을 씁니다. **인지도 최우선이면 `ofxQualisys`** 가 가장 무난합니다.

제 최종 추천 순위: **① ofxQTMRT** (정확·공식 정렬) → ② ofxQualisys (무난·검색용이) → ③ ofxQTM (짧지만 부정확).

`ofxQTMRT`로 갈까요? 아니면 안전하게 `ofxQualisys`?


## 🧑 User

ofxQTMRT로 가자


## 🤖 Assistant

`ofxQTMRT`로 확정. 만들겠습니다.


## 🤖 Assistant

> 🔧 `Bash` — Create addon dirs and copy SDK files


## 🤖 Assistant

Now the addon header:


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/gits/works/qualisys/ofxQTMRT/src/ofxQTMRT.h


## 🤖 Assistant

Now let me check how ofxNatNet builds its matrix and applies scale/transform, so I mirror its conventions exactly.

> 🔧 `Bash` — Inspect ofxNatNet matrix/scale handling


## 🤖 Assistant

> 🔧 `Bash` — Read ofxNatNet rigidbody matrix construction


## 🤖 Assistant

Now the implementation. Let me mirror ofxNatNet's single-`transform` scale convention (so `setScale`/`setTransform` behave identically).


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/gits/works/qualisys/ofxQTMRT/src/ofxQTMRT.cpp


## 🤖 Assistant

Now the `addon_config.mk` for cross-platform build, plus the example app:


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/gits/works/qualisys/ofxQTMRT/addon_config.mk


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/gits/works/qualisys/ofxQTMRT/example/addons.make


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/gits/works/qualisys/ofxQTMRT/example/src/main.cpp


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/gits/works/qualisys/ofxQTMRT/example/src/ofApp.h


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/gits/works/qualisys/ofxQTMRT/example/src/ofApp.cpp


## 🤖 Assistant

Let me verify the SDK actually compiles cleanly with the flattened addon include layout (this validates the self-compile setup on Linux):


## 🤖 Assistant

> 🔧 `Bash` — Compile-test SDK sources with addon include layout


## 🤖 Assistant

All 9 SDK sources compile cleanly against the flattened `include/` layout. Now let me syntax-check the addon's own `.cpp` — I'll stub the oF symbols the addon uses so I can catch logic/type errors against the SDK without a full openFrameworks tree:


## 🤖 Assistant

> 🔧 `Bash` — Syntax/type-check addon against SDK with oF stub


## 🤖 Assistant

> 🔧 `Bash` — Confirm addon object file built


## 🤖 Assistant

The addon compiles against the SDK. Now a README and an `addons.make`-friendly `.gitignore`, then a final summary:


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/gits/works/qualisys/ofxQTMRT/README.md


## 🤖 Assistant

> 🔧 `Bash` — Add license, gitignore, list final tree


## 🤖 Assistant

`ofxQTMRT` 완성했습니다. `/home/icq4ever/gits/works/qualisys/ofxQTMRT/`에 생성됐어요.

## 만든 것

- **[src/ofxQTMRT.h](ofxQTMRT/src/ofxQTMRT.h) / [.cpp](ofxQTMRT/src/ofxQTMRT.cpp)** — ofxNatNet 스타일 API. 내부 수신 스레드가 `CRTProtocol`로 연결→`StreamFrames(6d|3d|3dNoLabels)`→`Receive` 블로킹 루프를 돌리고, 더블버퍼로 메인스레드는 락 없이 읽음. 연결 끊기면 자동 재연결.
- **[addon_config.mk](ofxQTMRT/addon_config.mk)** — Windows만 `ws2_32`/`iphlpapi` 링크, C++14 설정. Linux/macOS는 추가 의존성 없음.
- **libs/qualisys_cpp_sdk/** — SDK 소스 9개 + 헤더 + tinyxml2를 `include/`·`src/`로 평탄화해서 자체 컴파일.
- **[example/](ofxQTMRT/example/)** — ofxNatNet 예제와 동일 구조의 마커+리지드바디 뷰어.
- **[README.md](ofxQTMRT/README.md)**, 번들 SDK 라이선스.

## 검증한 것

- SDK 소스 9개 전부 평탄화 include 레이아웃에서 **컴파일 성공**
- addon `.cpp`도 (oF 심볼 스텁으로) **SDK 대비 타입/문법 통과**

제공 API: `setup` / `update` / `setScale` / `setTransform` / `getNumRigidBody` / `getRigidBodyAt` / `getRigidBody(ById/ByName)` / `getMarker` / `getUnlabeledMarker` / `debugDraw`, 그리고 고급 기능 탈출구 `getProtocol()`.

## ⚠️ 직접 확인이 필요한 2가지

1. **회전행렬 변환** — QTM은 6DOF 회전을 column-major 3×3로 줍니다. oF의 row-vector `ofMatrix4x4` 규약으로 변환해뒀지만, **실제 QTM 하드웨어로 검증 못 했습니다.** 리지드바디 축이 거울처럼 뒤집혀 보이면 `buildRigidBody()` 주석대로 transpose로 채우면 됩니다.
2. **실 빌드** — oF 환경이 없어 풀 빌드는 못 했습니다. projectGenerator로 example 프로젝트 생성 후 `addons.make`에 `ofxQTMRT` 넣고 빌드해보시면 됩니다.

다음으로 원하시면: 스켈레톤 지원 추가, projectGenerator용 vcxproj/Makefile 생성, 또는 실제 QTM 연결 테스트를 도와드릴 수 있어요.


## 🧑 User

이거 trussc용 addon으로도 만들자 .. tcxQTMRT


## 🤖 Assistant

TrussC용 addon은 구조가 다르니, 먼저 TrussC 개발 규칙을 확인하겠습니다.


## 🤖 Assistant

> 🔧 `Skill` — trussc-dev


## 🧑 User

Base directory for this skill: /home/icq4ever/.claude/skills/trussc-dev

# TrussC Development Guide

Comprehensive environment for building apps with TrussC. This skill covers the full
loop: **design → implement → build → run → verify**. See topic files for detail.

## Quick Reference Files

- [architecture.md](architecture.md) — **How to structure an app**: Node hierarchy as code separation, Mods for cross-cutting behavior. Read before building anything beyond a single-file sketch.
- [node-system.md](node-system.md) — Node, RectNode, ScrollContainer, events, timers, mods (API reference)
- [graphics.md](graphics.md) — Drawing, colors, style, Image/Pixels/Texture/Fbo
- [app-lifecycle.md](app-lifecycle.md) — App setup, window, input, logging, threading, math
- [verify.md](verify.md) — **Runtime verification**: MCP screenshots, input injection, ImGui driving, evidence rules
- [addon-authoring.md](addon-authoring.md) — Creating & publishing addons (only when authoring one)

## The Development Loop

1. **Design** — structure the app as Node subclasses + Mods ([architecture.md](architecture.md)), not as one giant `tcApp::draw()`.
2. **Implement** — follow the patterns below. New/renamed/deleted source files → `trusscli update`.
3. **Build** — `trusscli build`. Fix errors before claiming anything.
4. **Verify** — launch with `TRUSSC_MCP=1`, screenshot, drive inputs, check state ([verify.md](verify.md)).
5. **Report** — verdict is **COMPLETE / COMPLETE WITH NOTES / BLOCKED** with an evidence table. A successful build alone is never COMPLETE.

## Build Workflow

### New Source File → re-update

When you create a new `.cpp` or `.h` file under `src/`, you MUST run:

```bash
trusscli update
```

TrussC uses `file(GLOB_RECURSE CONFIGURE_DEPENDS ...)`. Ninja picks up new files automatically, but Xcode does not. Always run `trusscli update` after adding/deleting/renaming source files.

### Build & Run

```bash
trusscli build                          # Build only
trusscli run                            # Build and launch
```

Always use `trusscli` instead of calling `cmake` directly. If `trusscli` can't do something you need, report it to the user.

### trusscli

If `CMakeLists.txt` or `CMakePresets.json` need regeneration (e.g., adding/removing addons):

```bash
trusscli update -p /path/to/project
```

Other common commands:
```bash
trusscli new /path/to/project           # Create new project
trusscli build                          # Build (native platform)
trusscli build --release                # Release config
trusscli run                            # Build and launch
trusscli clean                          # Delete build dirs
trusscli upgrade                        # Pull latest TrussC + rebuild trusscli
trusscli doctor                         # Check dev environment
trusscli info                           # Show project / framework info
```

## Addons (using them in your app)

Addons live in `addons/` next to the project. Before writing functionality from
scratch (networking, physics, CV, model loading…), **check whether an addon already
covers it.**

**Bundled with the framework** (in the TrussC repo's `addons/`):
tcxBox2d (2D physics), tcxCurl (HTTPS), tcxDepthCamera + tcxDepthRecord (depth cams),
tcxGltf / tcxObj (3D models), tcxHap (GPU video codec), tcxImGui (Dear ImGui),
tcxLua (scripting), tcxLut (color grading), tcxMidi (MIDI I/O),
tcxNodeInspector (runtime hierarchy + inspector + gizmo — Unity-style debug panel),
tcxOsc (OSC), tcxQuadWarp (projection mapping), tcxTls (TLS/SSL), tcxWebSocket.

**Community / registry** (cloned on demand): tcxArtnet (DMX/Art-Net), tcxAruco
(marker tracking), tcxAzureKinect, tcxGPT (OpenAI API), tcxGlitch (databending),
tcxIME (non-Latin text input), tcxMQTT, tcxOpenCV, tcxPhysics (Jolt 3D physics),
tcxPly (point clouds/meshes), tcxSyphon (macOS texture sharing) — and growing; the
list above goes stale, so query the registry for current truth.

### Querying the latest registry

The registry is a single JSON on the `trussc-addons` repo's gh-pages branch,
auto-rebuilt by a crawler (GitHub topic `trussc-addon`). `trusscli addon list
--remote` / `search` wrap it; fetch it directly when you want full metadata:

```bash
REG=https://raw.githubusercontent.com/TrussC-org/trussc-addons/gh-pages/registry.json

curl -s $REG | jq -r '.last_updated'                       # freshness stamp
curl -s $REG | jq -r '.addons[] | "\(.name)\t\(.description)"'
curl -s $REG | jq -r '.addons[] | select(.bundled|not) | .name'   # community only
curl -s $REG | jq '.addons[] | select(.category=="network")'
curl -s $REG | jq '.addons[] | select((.keywords|join(" "))+.description | test("midi";"i"))'
```

Per-addon fields: `name`, `owner`, `url`, `bundled`, `description`, `author`,
`license`, `category`, `keywords`, `platforms`, `screenshot`, `demo_url`,
`dependencies`, `stars`. Human-browsable version: https://trussc.org/addons/

```bash
trusscli addon list                     # Local addons + project status
trusscli addon list --remote            # Include registry (community)
trusscli addon search <query>           # Case-insensitive search of registry
trusscli addon add <addon>              # Add to project's addons.make
trusscli addon remove <addon>           # Remove from addons.make
trusscli addon clone <name>             # Clone from registry into addons/
trusscli addon clone <owner>/<name>     # Exact GitHub repo (disambiguates collisions)
trusscli addon clone <git-url>          # Direct HTTPS or git@host:user/repo SSH
trusscli addon pull --all               # git pull all cloned addons
```

Ambiguous bare names (e.g. a bundled `tcxLua` colliding with a community `funatsufumiya/tcxLua`) require `owner/name` form. Tab completion lists both forms.

**Writing your own addon?** → [addon-authoring.md](addon-authoring.md)

## Essential Patterns

### Namespace & Includes

```cpp
#include <TrussC.h>          // Angle brackets, not quotes
using namespace std;
using namespace tc;
// Omit std:: and tc:: prefixes everywhere
```

### Units & Constants

- **Angles:** TAU (= 2π, full rotation). NOT PI.
- **Colors:** 0.0–1.0 float range (not 0–255)
- **Logging:** `logNotice()`, `logWarning()`, `logError()` — NOT cout (stdout reserved for MCP)

### Event-Driven Rendering (redraw) — the GUI-app recipe

```cpp
setIndependentFps(60, EVENT_DRIVEN);  // update at a fixed 60, draw only on redraw()
```

This is the recommended mode for GUI/tool apps: a fixed update rate keeps timers,
tweens, and input polling deterministic regardless of monitor refresh (a 120 Hz
display won't double your update rate the way VSYNC does), while draw runs only
when something changed — an idle GUI costs near-zero GPU.

Call `redraw()` whenever display changes: key/mouse input, data updates, async load completion, window resize.

### Thread Safety

| Class | Main Thread | Background Thread |
|-------|:-----------:|:-----------------:|
| Pixels | OK | OK (CPU only) |
| Texture | OK | NO (GPU) |
| Image | OK | NO (GPU sync) |
| Fbo | OK | NO (GPU) |
| Font | OK | NO (GPU atlas) |

**Pattern:** Load `Pixels` in background thread → transfer to main thread → create Texture/Image.

### Node Hierarchy Basics

```cpp
auto child = make_shared<RectNode>();
child->setSize(100, 50);
child->setPos(10, 20);
parent->addChild(child);           // Automatic transform inheritance
child->enableEvents();             // Required for mouse events
child->setActive(false);           // Hides + disables update/draw/events
```

Draw order: `beginDraw()` → `draw()` → `drawChildren()` → `endDraw()`

Children draw in their parent's local coordinate space. No automatic clipping (use `setClipping(true)` on RectNode for scissor).

For *when and why* to split code into Nodes and Mods, see [architecture.md](architecture.md).

### Mouse Events

Override `onMousePress(Vec2 local, int button)` etc. Return `true` to consume (stop propagation).

Events are in **local coordinates** of the node. `enableEvents()` is required.

```cpp
bool onMousePress(Vec2 local, int button) override {
    if (button == 0) { /* left click */ return true; }
    return RectNode::onMousePress(local, button);  // propagate
}
```

### Timers

```cpp
uint64_t id = callAfter(0.5, [this]() { /* fires once after 0.5s */ });
uint64_t id2 = callEvery(1.0, [this]() { /* fires every 1s */ });
cancelTimer(id);
```

Protected Node methods. Store ID to cancel later.

## UI Widget Design Patterns

See [node-system.md](node-system.md) § "UI Widget Design Patterns" for details on:
- Every UI element as a RectNode child (labels, separators, buttons)
- Event<T>/EventListener RAII for widget communication (no raw `function<>` callbacks)
- PlainScrollContainer + ScrollBar + LayoutMod panel pattern
- Constructor/setup() separation (create in constructor, addChild in setup)
- Font rendering for all text

## Common Pitfalls

1. **Letter keys are UPPERCASE** → `'A'` not `'a'`. sokol uses key codes, not ASCII. `if (key == 'a')` will never match.
2. **sleep() inside mutex lock** → deadlock. Always sleep OUTSIDE lock scope.
3. **Image in background thread** → crash. Use Pixels for background, Image for main thread only.
4. **Texture update twice per frame** → second call silently skipped (sokol limitation).
5. **Forgot enableEvents()** → node won't receive mouse/key events.
6. **Forgot redraw()** → screen doesn't update in event-driven mode.
7. **setLineWidth()** → doesn't exist in sokol. drawLine is always 1px. For thick lines: `Path::drawStroke()` (uses StrokeMesh internally), or assemble StrokeMesh directly.
8. **std::map vs tc::map** → name collision with `using namespace tc`. Use `std::map` explicitly if needed, or avoid `using namespace tc` in that file.
9. **Copy Pixels/Image/Texture** → deleted copy constructor. Use `std::move()` or `Pixels::clone()`.
10. **Node without make_shared** → `addChild()` fails. Always create nodes with `make_shared<>()`.
11. **getGlobalPos()** → returns `Vec3` (origin of the node in global space). Shorthand for `localToGlobal(Vec3(0,0,0))`.
12. **removeChild() during iteration** → crashes (vector mutation). Use `destroy()` for deferred removal. Check `isDead()` to skip destroyed nodes.
13. **addChild() in constructor** → crash. `weak_from_this()` fails because `shared_ptr` isn't complete yet. Always `addChild()` in `setup()` override.
14. **Raw `function<>` callbacks** → no auto-cleanup, dangling risk. Use `Event<T>` + `EventListener` (RAII auto-disconnect on destruction).
15. **Drawing UI elements in parent draw()** → won't scroll, no hit testing. Make every UI element a RectNode child.
16. **LayoutMod auto-relayout** → doesn't happen. Call `updateLayout()` manually in `setSize()` and after child changes.
17. **IBL/`Environment` on iOS Safari (wasm)** → auto-skipped. The IBL bake renders into cube-face render targets, which iOS/iPadOS Safari (both WebGPU & WebGL2) can't do without breaking the canvas swapchain (one frame then black/frozen; loop keeps running). So `loadProcedural()`/`loadFromHDR()` no-op on iOS → PBR falls back to flat hemisphere ambient + direct lights. Plain 2D FBOs, normal maps, shadows are fine; native, desktop web and Android web bake IBL normally. Don't rely on IBL reflections for cross-platform looks if you target iOS web.
18. **setBlendMode() inside a 3D scene kills depth** → EVERY blend pipeline (Alpha, Add, even Disabled) has depth write/test OFF; only `internal::pipeline3d` (loaded by screen setup / `EasyCam::begin()`) writes depth. "Restoring" with `setBlendMode(Alpha)` after an additive effect leaves the rest of the scene depth-less — geometry renders in submission order, so a box's unlit BOTTOM face overwrites its lit front face (looks like a pitch-black face / lights mysteriously having no effect). Restore depth explicitly: `setBlendMode(BlendMode::Alpha); if (internal::pipeline3dInitialized) sgl_load_pipeline(internal::pipeline3d);`. See graphics.md § Blend Modes.
18b. **`random()` collides with POSIX** → use `tc::random(min, max)` explicitly (there is no `randomf`). Unqualified `random()` can resolve to the C library function and compile to the wrong thing silently.
18c. **EasyCam starts at elevation 0 (true side view)** → flat scenes (orbits, ground planes) look like a line on first launch. Set an initial angle explicitly: `cam.setElevation(...)` / `cam.setAzimuth(...)`.
19. **EasyCam `enableMouseInput()` eats every left-press** → it consumes the press as orbit-start during `events().mousePressed`, and tree dispatch only runs `if (!consumed)` — all RectNode/Node mouse handlers silently stop working while app-level `mousePressed()` virtuals still fire (confusing!). With clickable nodes, either skip `enableMouseInput()`, or move orbit to `setOrbitButton(right)` / `setDragModifier(shift)`. Debug trick: `get_selected_node` after an MCP `mouse_click` — null selection on a known RectNode proves dispatch never ran.


## 🤖 Assistant

TrussC addon 작성 규칙과 기존 addon 예시(tcxNozzle)를 함께 확인하겠습니다.


## 🤖 Assistant

> 🔧 `Bash` — Read addon authoring guide


## 🤖 Assistant

> 🔧 `Bash` — Inspect existing TrussC addon structure


## 🤖 Assistant

> 🔧 `Read` — /home/icq4ever/TrussC/addons/tcxNozzle/src/tcxNozzle.h


## 🤖 Assistant

> 🔧 `Read` — /home/icq4ever/TrussC/addons/tcxNozzle/CMakeLists.txt


## 🤖 Assistant

TrussC의 수학 타입(Vec3/Mat4/Quat)과 그리기 API를 확인하겠습니다.


## 🤖 Assistant

> 🔧 `Bash` — Locate TrussC math headers


## 🤖 Assistant

> 🔧 `Bash` — Inspect TrussC math types


## 🤖 Assistant

> 🔧 `Bash` — Inspect Mat4 and Quaternion API


## 🤖 Assistant

> 🔧 `Bash` — Read full Mat4 definition


## 🤖 Assistant

TrussC `Mat4`는 **열벡터(column-vector) 규약 + 행우선 저장**이라 oF(행벡터)와 반대 — 회전행렬 매핑을 그에 맞게 바꿔야 합니다. 그리기 API와 카메라 idiom도 확인할게요.


## 🤖 Assistant

> 🔧 `Bash` — Find TrussC draw + camera API


## 🤖 Assistant

> 🔧 `Bash` — Find multMatrix and 3D primitive draw functions


## 🤖 Assistant

> 🔧 `Bash` — Check multMatrix and find 3D example


## 🤖 Assistant

> 🔧 `Bash` — Read projector sim example for camera+draw idiom


## 🤖 Assistant

> 🔧 `Bash` — Confirm text/background/box draw APIs


## 🤖 Assistant

> 🔧 `Bash` — Find background and drawBox signatures


## 🤖 Assistant

> 🔧 `Bash` — Locate drawBox and background functions


## 🤖 Assistant

> 🔧 `Bash` — Find drawBox definition file and background usage


## 🤖 Assistant

> 🔧 `Bash` — Inspect TrussC.h global draw fns


## 🤖 Assistant

SDK 빌드/그리기 API 모두 확인했습니다. 이제 기존 addon의 example 구조를 참고하겠습니다.


## 🤖 Assistant

> 🔧 `Bash` — Find an addon example-* layout to mirror


## 🤖 Assistant

A mocap addon이 이미 있네요 (`tcxFSportsMocap`). 참고하고, OSC addon의 example/CMake/addon.json 패턴도 확인하겠습니다.


## 🤖 Assistant

> 🔧 `Bash` — Inspect existing mocap addon


## 🤖 Assistant

> 🔧 `Bash` — Read OSC addon example + metadata patterns


## 🤖 Assistant

`tcxFSportsMocap`은 OSC 경유 mocap 툴킷이라 직접 QTM 수신과는 다릅니다 — 중복 아님. 이제 만들겠습니다. (addon은 빌드 가능하도록 `TrussC/addons/`에 배치)


## 🤖 Assistant

> 🔧 `Bash` — Create tcxQTMRT dirs and vendor SDK


## 🤖 Assistant

좋은 아이디어인데, **어디에 넣느냐**가 설계상 중요해서 잠깐 짚을게요. (아직 addon 소스 작성 전이라 지금 정하면 깔끔합니다)

QTM 수신 → OSC 송신은 실무에서 흔한 파이프라인이에요 (QTM을 TouchDesigner/Max/다른 머신으로 중계). 게다가 이 저장소엔 이미 **`tcxFSportsMocap`이 OSC로 mocap을 받는** 구조라, QTM→OSC 브리지를 만들면 그쪽에 바로 물립니다.

다만 **핵심 클래스(`tcx::QTMRT`)에 OSC 송신을 직접 박는 건 권하지 않습니다:**

- 단일 책임 위반 — "QTM 수신"과 "OSC 송신"은 별개 관심사
- `tcxQTMRT`가 `tcxOsc`에 **강제 의존** → OSC 안 쓰는 사용자도 끌려옴
- TrussC addon 철학은 **조합(composition)** — addons.make에 `tcxQTMRT tcxOsc` 둘 다 적고 앱에서 엮는 게 정석

그래서 세 가지 방식이 있어요:


## 🤖 Assistant

> 🔧 `AskUserQuestion`


## 🤖 Assistant

브리지 예제 이름 `qtmrt2osc`로 하겠습니다 (→ `example-qtmrt2osc/`). 핵심 클래스는 순수 QTM 수신, 브리지는 별도 예제로 조합. 먼저 핵심 addon을 작성합니다.


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/TrussC/addons/tcxQTMRT/src/tcxQTMRT.h


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/TrussC/addons/tcxQTMRT/src/tcxQTMRT.cpp


## 🤖 Assistant

좋은 질문이에요 — 두 개 다 핵심을 찌릅니다.

## 1. QTM은 리눅스용이 있나?

**없습니다. QTM(Qualisys Track Manager) 서버 소프트웨어는 Windows 전용이에요.** 네이티브 Linux/macOS 버전이 없습니다.

하지만 그게 우리 addon의 크로스플랫폼에는 **문제가 안 됩니다.** 왜냐하면:

- **QTM(Windows)** = 카메라 잡고 트래킹하는 *서버/호스트*
- **qualisys_cpp_sdk = 우리 addon** = 네트워크로 데이터를 받는 *클라이언트*

클라이언트(우리 코드)는 Linux/macOS/Windows 다 돌아갑니다. 즉 **리눅스 TrussC 앱이 네트워크 너머 Windows QTM 박스에 접속**하는 구조예요. 앞서 "빌드 타겟 3개 다"가 유효한 이유가 이거고요.

## 2. Motive랑 다른가? → 거의 똑같습니다

당신 직감이 정확해요. **아키텍처가 OptiTrack과 사실상 동일**합니다:

| 역할 | OptiTrack | Qualisys |
|---|---|---|
| 서버 (Windows 전용) | **Motive** | **QTM** |
| 스트리밍 프로토콜 | **NatNet** | **QTM RT Protocol** |
| 클라이언트 SDK | NatNet SDK / ofxNatNet | qualisys_cpp_sdk / 우리 addon |

둘 다 "Windows 서버가 카메라로 트래킹 → 네트워크로 프로토콜 송출 → 클라이언트가 수신"하는 똑같은 모델입니다. 그래서 ofxNatNet 사용하던 사람이 macOS에서 Windows Motive에 붙던 것처럼, **리눅스에서 Windows QTM에 붙는** 것도 동일하게 됩니다.

차이라면 NatNet은 멀티캐스트 UDP 중심, QTM RT는 TCP 명령포트(22222) + UDP 데이터 스트림이라는 전송 방식 정도고, 데이터 의미(리지드바디·마커)는 같습니다.

---

이건 작업에 영향 없으니, 계속 진행해서 CMakeLists/예제 만들고 빌드 검증하겠습니다.


## 🤖 Assistant

> 🔧 `Bash` — Inspect tcxOsc sender API for bridge example


## 🤖 Assistant

> 🔧 `Bash` — Find OscSender send method signatures


## 🤖 Assistant

Now the CMakeLists, metadata, and both examples:


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/TrussC/addons/tcxQTMRT/CMakeLists.txt


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/TrussC/addons/tcxQTMRT/addon.json


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/TrussC/addons/tcxQTMRT/example-basic/addons.make


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/TrussC/addons/tcxQTMRT/example-basic/CMakeLists.txt


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/TrussC/addons/tcxQTMRT/example-basic/src/main.cpp


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/TrussC/addons/tcxQTMRT/example-basic/src/tcApp.h


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/TrussC/addons/tcxQTMRT/example-basic/src/tcApp.cpp


## 🤖 Assistant

Now the `qtmrt2osc` bridge example (combines tcxQTMRT + tcxOsc):


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/TrussC/addons/tcxQTMRT/example-qtmrt2osc/addons.make


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/TrussC/addons/tcxQTMRT/example-qtmrt2osc/CMakeLists.txt


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/TrussC/addons/tcxQTMRT/example-qtmrt2osc/src/main.cpp


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/TrussC/addons/tcxQTMRT/example-qtmrt2osc/src/tcApp.h


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/TrussC/addons/tcxQTMRT/example-qtmrt2osc/src/tcApp.cpp


## 🤖 Assistant

Now LICENSE, README, then build-verify the basic example:


## 🤖 Assistant

> 🔧 `Bash` — Write LICENSE files


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/TrussC/addons/tcxQTMRT/README.md


## 🤖 Assistant

좋은 질문이에요. 위치 자체는 **맞습니다** — 바로 옆 `tcxNozzle`이 정확히 같은 패턴이거든요: 서드파티 라이브러리를 addon 루트에 두고(`tcxNozzle/nozzle/`) `add_subdirectory`로 빌드. 우리도 `tcxQTMRT/qualisys_cpp_sdk/` + `add_subdirectory`라 일관됩니다. (TrussC는 oF처럼 `libs/`에 넣는 관례가 아니라 루트에 두는 쪽이에요.)

다만 두 가지 개선점이 있어요. 먼저 실제로 뭐가 들어갔는지 보죠:


## 🤖 Assistant

> 🔧 `Bash` — Inspect vendored SDK for cruft


## 🤖 Assistant

확인해보니 위치는 맞는데 **불필요한 파일이 같이 딸려왔어요** (예제·테스트·Visual Studio 솔루션 ~480K+). SDK의 CMakeLists가 이것들을 `BUILD_EXAMPLES/BUILD_TESTS` 옵션(우리가 OFF로 설정)으로 가드해서 빌드에 끼진 않지만, addon에 죽은 무게라 정리하는 게 좋습니다:


## 🤖 Assistant

> 🔧 `Bash` — Trim SDK cruft and verify CMakeLists consistency


## 🤖 Assistant

위치 정리 완료 — 제거한 디렉토리 참조는 모두 `BUILD_EXAMPLES`/`BUILD_TESTS`(OFF) 가드 안이라 안전합니다.

이제 실제로 빌드 검증하겠습니다 (TrussC + SDK + addon + 예제 전체 컴파일):


## 🤖 Assistant

> 🔧 `Bash` — Run trusscli update on example-basic


## 🤖 Assistant

Configure succeeded (addon + SDK recognized). Now the actual build:


## 🤖 Assistant

> 🔧 `Bash` — Build example-basic


## 🤖 Assistant

빌드가 핵심 문제를 드러냈어요: addon이 `<TrussC.h>` 전체를 포함하니 TrussC 헤더의 템플릿 인스턴스화 에러가 터집니다. `tcxNozzle`이 `<TrussC.h>`를 안 쓰고 전방선언만 한 이유가 이거였네요. 수학 타입만 가볍게 포함하도록 고치겠습니다.


## 🤖 Assistant

> 🔧 `Bash` — Check tcMath standalone + locate log API


## 🤖 Assistant

핵심 발견: 타입은 `trussc::` 네임스페이스에 있고 `tc` 별칭은 `TrussC.h`에만 있습니다. `tcxNozzle`처럼 수학/로그 헤더만 포함하고 별칭은 직접 선언하겠습니다.


## 🤖 Assistant

> 🔧 `Edit` — /home/icq4ever/TrussC/addons/tcxQTMRT/src/tcxQTMRT.h


## 🤖 Assistant

IDE 진단은 캐시된 것(include 경로 미반영)이라 실제 빌드로 판단하겠습니다. `.cpp`도 로그 헤더를 가볍게 포함하도록 고칩니다:


## 🤖 Assistant

> 🔧 `Edit` — /home/icq4ever/TrussC/addons/tcxQTMRT/src/tcxQTMRT.cpp


## 🤖 Assistant

IDE 진단(clangd가 include 경로/표준 라이브러리를 못 잡음)은 신뢰할 수 없으니 실제 빌드로 확인합니다:


## 🤖 Assistant

> 🔧 `Bash` — Rebuild example-basic after include fix


## 🤖 Assistant

이제 진짜 원인이 드러났습니다: TrussC 헤더가 **C++20**(`requires` 콘셉트)을 요구하는데 addon CMakeLists를 C++17로 설정했어요. C++20으로 올리겠습니다 (SDK의 C++14는 하위호환이라 무방):


## 🤖 Assistant

> 🔧 `Edit` — /home/icq4ever/TrussC/addons/tcxQTMRT/CMakeLists.txt


## 🤖 Assistant

> 🔧 `Bash` — Rebuild with C++20


## 🤖 Assistant

`example-basic` 빌드 성공 (남은 건 TrussC 코어 자체의 deprecation 경고뿐). 이제 OSC 브리지 예제도 빌드 검증합니다:


## 🤖 Assistant

> 🔧 `Bash` — Build qtmrt2osc bridge example


## 🤖 Assistant

둘 다 빌드 성공. 런타임에서 창이 뜨고 크래시 없이 렌더되는지 짧게 확인하겠습니다 (QTM 서버는 없으니 "connected: NO" 상태로 렌더만 검증):


## 🤖 Assistant

> 🔧 `Bash` — Attempt brief runtime launch


## 🤖 Assistant

앱이 디스플레이(:0)에서 크래시 없이 실행됐습니다. verify.md 방식으로 스크린샷 증거를 남기겠습니다.


## 🤖 Assistant

> 🔧 `Bash` — Read verify.md for MCP screenshot mechanism


## 🤖 Assistant

Let me launch with the MCP endpoint and capture a screenshot to verify it renders:


## 🤖 Assistant

> 🔧 `Bash` — Launch with MCP and screenshot


## 🤖 Assistant

> 🔧 `Read` — /tmp/qtm_verify.png


## 🤖 Assistant

> 🔧 `Bash` — Quit app and clean up


## 🤖 Assistant

정리 완료. 스크린샷으로 확인됐습니다 — 3D 씬(원점 축 3개)과 오버레이("connected: NO, frame 0, rigid bodies 0…")가 정상 렌더되고, 백그라운드 연결 스레드가 렌더 루프를 막지 않습니다.

## tcxQTMRT 완성 ✅

`/home/icq4ever/TrussC/addons/tcxQTMRT/`에 TrussC addon으로 만들었습니다.

**구조** (tcxNozzle 패턴 그대로):
- [src/tcxQTMRT.h](TrussC/addons/tcxQTMRT/src/tcxQTMRT.h) / [.cpp](TrussC/addons/tcxQTMRT/src/tcxQTMRT.cpp) — `tcx::QTMRT` 클래스. pImpl로 SDK를 숨기고, 백그라운드 스레드 + 더블버퍼. `tc::Vec3`/`tc::Mat4` 네이티브 타입.
- `qualisys_cpp_sdk/` — 벤더링된 SDK(`add_subdirectory`로 자체 빌드). 불필요한 예제/테스트/VS 솔루션 파일은 정리.
- [CMakeLists.txt](TrussC/addons/tcxQTMRT/CMakeLists.txt), [addon.json](TrussC/addons/tcxQTMRT/addon.json), README, LICENSE.
- **example-basic** — 마커/리지드바디 3D 뷰어.
- **example-qtmrt2osc** — QTM → OSC 브리지 (`tcxQTMRT` + `tcxOsc` 조합, `/qtm/rigidbody …` 송출). FSportsMocap 같은 OSC 소비자에 바로 연결.

**검증** (verify.md 기준):

| 항목 | 결과 | 증거 |
|---|---|---|
| addon + SDK 빌드 (Linux) | ✅ | `libtcxQTMRT.a`, `libqualisys_cpp_sdk.a` 링크 |
| example-basic 빌드 | ✅ | `Built target example-basic` |
| example-qtmrt2osc 빌드 | ✅ | `Built target example-qtmrt2osc` |
| 런타임 렌더링/생명주기 | ✅ | MCP 스크린샷 — 씬+오버레이 정상, 크래시 없음 |

**해결한 문제**: addon이 `<TrussC.h>` 전체를 포함하면 안 됨(App 템플릿 인스턴스화 충돌) → 수학/로그 헤더만 포함. 그리고 TrussC 코어는 **C++20**(concepts) 필요 → addon을 C++20으로.

## 판정: COMPLETE WITH NOTES

검증 못 한 부분(라이브 QTM 하드웨어 필요):
1. **회전행렬 변환** — QTM column-major 3×3 → TrussC column-vector `Mat4` 매핑은 논리적으로 맞췄지만 실제 데이터로 미검증. 축이 거울처럼 뒤집히면 `buildMatrix()`에서 transpose. (oF 버전과 반대 매핑인 이유: oF는 행벡터, TrussC는 열벡터 규약)
2. **실제 데이터 경로**(리지드바디/마커 수신) — QTM 서버 없으니 "connected: NO"까지만 확인.
3. **macOS/Windows 빌드** — Linux만 빌드. Windows는 SDK가 `ws2_32`/`iphlpapi`를 자동 전파하도록 돼 있음.

다음으로 원하시면: 스켈레톤 지원 추가, 또는 실제 QTM(혹은 QTM 데모/시뮬레이터)에 붙여서 회전·데이터 검증을 도와드릴 수 있어요.


## 🧑 User

지금 대화내용, tcxQTMRT에서 이어지도록 할수 있나 ?


## 🤖 Assistant

네, 가능합니다. 가장 확실한 방법은 **addon 디렉토리에 `CLAUDE.md`를 두는 것**이에요 — Claude Code가 그 폴더에서 실행될 때 자동으로 읽어들여서, 새 세션이라도 지금까지의 결정·상태·미해결 항목을 그대로 이어받습니다. (메모리는 현재 프로젝트 경로 `gits/works/qualisys`에 묶여 있어서, `TrussC/addons/tcxQTMRT`를 따로 열면 안 따라옵니다. 그래서 repo 안의 문서가 더 견고해요.)

지금까지의 맥락을 정리해서 작성하겠습니다:


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/TrussC/addons/tcxQTMRT/CLAUDE.md


## 🤖 Assistant

`CLAUDE.md`를 작성했습니다. 이제 `TrussC/addons/tcxQTMRT/`에서 Claude Code를 열면 이 맥락(결정 사항·해결한 함정·미검증 항목·빌드법)이 자동으로 로드됩니다.

원래 프로젝트(`gits/works/qualisys`)에서도 이어갈 수 있게, 두 addon의 관계를 메모리에 짧게 남겨두겠습니다:


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/.claude/projects/-home-icq4ever-gits-works-qualisys/memory/qtmrt-addons.md


## 🤖 Assistant

> 🔧 `Bash` — Check MEMORY.md index


## 🤖 Assistant

> 🔧 `Write` — /home/icq4ever/.claude/projects/-home-icq4ever-gits-works-qualisys/memory/MEMORY.md


## 🤖 Assistant

완료했습니다. 두 곳에서 맥락이 이어집니다:

1. **`TrussC/addons/tcxQTMRT/CLAUDE.md`** — 그 폴더에서 Claude Code를 열면 자동 로드됩니다. 핵심 결정(이름·SDK 벤더링·OSC 분리), 이미 해결한 함정(`<TrussC.h>` 직접 포함 금지, C++20 필수), 미검증 항목(회전 매핑·라이브 데이터·macOS/Windows), 빌드/검증 커맨드까지 담겨 있어요.

2. **프로젝트 메모리** (`gits/works/qualisys`) — `ofxQTMRT`/`tcxQTMRT` 두 addon의 위치와 관계, tcxQTMRT 세부는 그쪽 CLAUDE.md를 보라는 포인터.

이제 사용하는 방법:
- **TrussC addon 작업 이어가기**: `cd ~/TrussC/addons/tcxQTMRT` 후 Claude Code 실행 → CLAUDE.md 자동 반영.
- **이 프로젝트에서 이어가기**: 메모리가 두 addon을 가리켜 줍니다.

한 가지 참고: CLAUDE.md/메모리는 "무엇을·왜" 했는지의 요약이지, 이 대화의 토큰 단위 전체 기록은 아니에요. 결정과 미해결 항목 위주라 실제 작업 재개엔 충분합니다. 혹시 대화 트랜스크립트 원문 자체를 addon repo에 남기고 싶으면 그것도 파일로 떨궈둘 수 있는데, 그렇게 할까요?


## 🧑 User

응 그렇게 해줘


## 🤖 Assistant

트랜스크립트 원문을 찾아서 addon repo에 넣겠습니다. 먼저 이 세션의 기록 파일을 찾습니다:


## 🤖 Assistant

> 🔧 `Bash` — Locate session transcript jsonl


## 🤖 Assistant

세션 트랜스크립트를 찾았습니다 (1.1M, 방금 수정 = 이 세션). 원문 jsonl을 그대로 넣고, 읽기 쉬운 대화 마크다운도 함께 만들겠습니다. 먼저 구조를 확인합니다:


## 🤖 Assistant

> 🔧 `Bash` — Inspect transcript jsonl structure


## 🤖 Assistant

구조 파악 완료. 원문 jsonl을 그대로 복사하고, 읽기 쉬운 대화 마크다운을 함께 생성합니다:


## 🤖 Assistant

> 🔧 `Bash` — Copy raw jsonl and render readable markdown

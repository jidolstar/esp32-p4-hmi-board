# S07 상세 계획: Wi-Fi AP & Web Control Page

작성: 2026-11-08. 상세화: 2026-11-11. 상태: `complete`. A0~A5 `complete`.
선행 조건: S06 V4 자동 build·memory gate `complete`. S06 V5 실기기 수용은
2026-11-10 사용자 요청으로 후속 gate로 유보했다.

## 1. 목적과 결정 요약

HMI에서 임시 Wi-Fi AP를 시작하고, 휴대폰이 AP에 연결한 뒤 로컬 웹 화면에서 네 개의
고정 switch를 조작할 수 있게 한다. HMI와 웹은 같은 canonical state를 사용하며 어느
쪽에서 바꿔도 다른 쪽에 즉시 반영한다.

1. A0에서 AP-only DHCP와 `/healthz` 실기기 경로를 확인했고, A0.5에서 기존 STA를
   유지하는 AP+STA 공존과 양쪽 data path를 실기기로 확인했다. AP+STA를 기본 정책으로
   확정하고 AP-only suspend/restore는 fallback으로만 유지한다.
2. 화면은 `AP 시작 → Wi-Fi 연결 → 웹 열기 → 4개 switch 제어` 흐름을 한 page 안에서 안내한다.
3. Wi-Fi QR과 URL QR을 동시에 보여 주지 않고 현재 단계에 필요한 QR 하나만 크게 표시한다.
4. HMI와 웹은 controller가 소유하는 단일 4-switch state를 command/snapshot으로만 사용한다.
5. AP 실행 중 Back은 종료 확인을 거치고 AP/server 정리 뒤 Dashboard로 이동한다.
6. locale/theme 재구성은 AP, client와 switch state를 재시작하거나 초기화하지 않는다.
7. 웹 자산은 firmware 고정 bundle로 제공하며 SD, 외부 CDN과 인터넷 연결에 의존하지 않는다.
8. Codex는 firmware를 build까지만 수행한다. upload와 휴대폰 실기기 검증은 사용자가 수행한다.

## 2. A0 착수 전 상태와 구현 전제

- `modules/connectivity/`에는 STA scan, credential, 저장·복구 정책과 작은 snapshot이 있다.
- `platform/hmi/connectivity/esp_hosted_wifi_sta.*`는 ESP-Hosted `2.12.8`,
  esp_wifi_remote `1.6.0`을 compile-time으로 고정하고 `WIFI_STA`만 시작한다.
- `platform/windows/connectivity/virtual_wifi_sta.*`는 실제 PC 네트워크를 변경하지 않는
  결정적 STA fixture다.
- `hosts/hmi/main.cpp`와 `hosts/windows/main.cpp`는 `WifiStaRegistration`을 한 번 생성해
  `AppContext`로 전달한다.
- `samples/suite/page_contract.hpp`에는 AP/web controller port가 없다.
- `samples/suite/page_registry.json`에는 S01~S06만 등록돼 있다.
- `samples/suite/i18n/`은 한국어/영어 catalog와 fixed typography 생성 계약을 가진다.
- 현재 framework에 AP 관련 header와 `esp_http_server` binary가 있어도 실제 SoftAP data
  path, DHCP, socket과 restart 성공을 보장하는 근거로 간주하지 않는다.
- QR encoder, HTTP/WebSocket transport와 embedded web bundle 도구는 아직 없다.

따라서 기존 STA adapter에 AP 호출만 덧붙이지 않는다. 하나의 physical radio를 STA와 AP가
동시에 임의로 변경하지 못하도록 mode ownership부터 만든다.

## 3. 범위와 제외 범위

### 포함

- 임시 WPA2 이상 AP 시작·중지와 AP+STA 공존 우선 정책
- 공존 불가 판정 시 AP-only 전환 fallback
- SSID, 암호, AP IPv4, client 수와 server 상태 표시
- Wi-Fi 접속 QR과 local URL QR의 단계별 표시
- HMI와 mobile web의 고정 switch 네 개 양방향 동기화
- WebSocket 초기 snapshot, command, broadcast와 reconnect
- locale/theme 전환, page 왕복, app destroy와 AP+STA 수명 처리
- Windows 결정적 fixture와 mobile web 검증
- HMI build, RAM/flash/stack/queue/bundle 크기 기록

### 제외

- 인터넷 공유, NAT와 router 기능
- captive portal 자동 열기 보장
- 임의 GPIO 제어, shell, file browser, firmware/asset update endpoint
- 사용자 정의 switch 개수·이름·automation
- cloud 연결, account와 TLS 인증서 배포
- camera/gallery 접근
- 인터넷 공유와 동일시한 AP+STA 운용

## 4. UX 목표와 원칙

### 4.1 항상 답해야 하는 세 질문

화면은 어느 상태에서도 다음 질문에 답해야 한다.

1. 지금 무엇을 해야 하는가.
2. 휴대폰 연결이 어디까지 진행됐는가.
3. AP를 종료하면 어떤 연결이 끊기는가.

### 4.2 정보 구조

- 연결은 `1. Wi-Fi 연결`과 `2. 웹 열기` 두 단계로 보인다.
- 현재 단계의 QR 하나만 220 px 이상으로 표시한다.
- SSID, 암호, URL과 client 수는 label/value 행으로 분리한다.
- 완료된 단계는 접고 완료 badge를 표시하되 `Wi-Fi QR 다시 보기`로 돌아갈 수 있게 한다.
- switch 네 개는 page 이동 없이 한눈에 보이고 카드 전체가 touch target이다.
- 기술 code보다 사용자가 할 수 있는 다음 action을 먼저 보여 준다.

### 4.3 조작 안전성

- `AP 시작`은 primary, `AP 종료`는 danger action으로 구분한다.
- client 연결 중 종료 또는 Back은 확인 dialog를 표시한다.
- AP 시작·중지 중에는 중복 action을 막고 spinner와 현재 작업을 표시한다.
- AP-only fallback에서만 STA 복구 상태와 오류를 표시한다.
- QR은 button처럼 보이지 않게 하며 충분한 quiet zone을 둔다.
- 상태는 색만으로 구분하지 않고 text, icon/badge와 위치를 함께 사용한다.

## 5. 사용자 흐름

### 5.1 AP 꺼짐

1. Dashboard에서 `Wi-Fi AP 웹 제어`를 연다.
2. 기능 목적, 3단계 흐름과 AP+STA 공존 안내를 확인한다.
3. 기존 STA 연결은 유지되지만 AP client에 인터넷을 공유하지는 않는다고 표시한다.
4. `AP 시작`을 누른다.

### 5.2 AP 시작 중

1. start action을 disable하고 progress indicator를 표시한다.
2. 기존 STA를 유지한 채 AP, DHCP와 server를 순서대로 시작한다.
3. 15초 안에 ready가 되지 않으면 timeout 오류와 `다시 시도`를 제공한다.

### 5.3 1단계: 휴대폰 Wi-Fi 연결

1. connection card에 Wi-Fi QR을 표시한다.
2. QR 아래에 SSID, 암호와 `휴대폰 Wi-Fi 설정에서 연결하세요.`를 표시한다.
3. 암호는 기본 표시하되 `암호 숨기기/표시`를 제공한다.
4. client 상태가 500 ms 이상 안정되면 2단계로 한 번만 자동 전환한다.

### 5.4 2단계: 로컬 웹 열기

1. 같은 위치에 URL QR을 표시한다.
2. URL과 `인터넷 연결 없이 이 기기에서만 열립니다.`를 함께 보여 준다.
3. WebSocket 인증 완료 시 `웹 연결됨`으로 바뀐다.
4. `Wi-Fi QR 다시 보기`는 AP를 유지한 채 표시 단계만 바꾼다.

### 5.5 3단계: 네 switch 제어

1. HMI switch는 AP ready부터 조작할 수 있다.
2. 웹이 연결되면 authoritative snapshot으로 상태를 맞춘다.
3. 어느 쪽의 변경도 300 ms 안에 반대쪽 시각 상태 갱신을 목표로 한다.
4. stale command는 적용하지 않고 최신 snapshot을 다시 보내 상태를 수렴시킨다.
5. client가 끊겨도 HMI switch state는 유지하고 웹 상태만 `재연결 대기`로 바꾼다.

### 5.6 종료와 STA 유지

1. `AP 종료` 또는 확인된 Back이 AP/server stop을 요청한다.
2. 새 web command 차단 → client close → server stop → DHCP/AP stop 순서로 정리한다.
3. 기존 STA를 건드리지 않고 Dashboard로 이동한다.
4. AP-only fallback을 사용한 경우에만 저장된 STA 복구와 실패 처리를 수행한다.

## 6. 1024×600 HMI layout

### 6.1 공통 치수

- root padding: 14 px
- header: 62 px, 기존 Back/title/KO/EN/theme control 유지
- body: 남은 영역 전체, 전체 화면 scroll 없음
- primary/danger action: 높이 56 px 이상
- switch card: 최소 높이 132 px, 카드 전체 touch 가능
- 일반 touch target: 52×52 px 이상
- 기존 shell header control: 기존 44 px 계약 유지
- multiline Korean/English: line spacing 5 px 이상 명시

### 6.2 AP 꺼짐과 오류

body 중앙에 최대 720×360 px의 onboarding card를 둔다.

| 영역 | 내용 |
| --- | --- |
| 상단 | 기능 제목과 한 줄 목적 |
| 중앙 | `휴대폰 연결 → 웹 열기 → 4개 switch 제어` 3단계 설명 |
| 안내 | AP+STA 공존과 AP client 인터넷 미공유 안내 |
| 하단 | `AP 시작` 또는 오류의 `다시 시도` |

오류는 `Wi-Fi AP를 시작하지 못했습니다.`처럼 사용자 문구를 먼저 보여 주고 다음 줄에
`장치를 다시 시도하거나 Wi-Fi 설정으로 돌아가세요.`처럼 복구 action을 설명한다.

### 6.3 AP 실행 상태

body는 360 px connection column과 나머지 control column의 2열 layout을 사용한다.

```text
+---------------- 360 ----------------+  +--------------- 624 ----------------+
| 단계/연결 상태                      |  | 웹 제어 · 연결된 기기 1                 |
|                                     |  |                                       |
|          220 px QR                  |  | [ Switch 1 ]      [ Switch 2 ]         |
|                                     |  |                                       |
| SSID / 암호 또는 URL                |  | [ Switch 3 ]      [ Switch 4 ]         |
| 단계 전환 보조 action               |  |                                       |
| AP 종료                             |  | 동기화/재연결 상태                     |
+-------------------------------------+  +---------------------------------------+
```

- column gap은 12 px다.
- connection card는 QR이 잘리지 않도록 고정 폭을 사용한다.
- control column은 2×2 switch grid와 하단 status strip으로 구성한다.
- switch label과 `켜짐`/`꺼짐`은 별도 label로 두고 색과 위치를 함께 바꾼다.
- title/body height를 측정해 고정하고 본문은 최대 두 줄로 작성한다.
- locale에 따라 문장이 길어져도 QR과 primary action을 밀어내지 않게 한다.

### 6.4 QR 렌더링

- theme와 무관하게 흰 배경·검은 module을 사용한다.
- 4-module quiet zone, integer scale과 nearest-neighbor rendering을 사용한다.
- payload 최대 192 bytes, QR scratch buffer 최대 1 KiB를 목표로 한다.
- Wi-Fi QR: `WIFI:T:WPA;S:...;P:...;;`
- URL QR: `http://<ap-ip>/#t=<session-token>`
- token은 URL fragment에 두어 일반 HTTP request와 log에 남기지 않는다.
- QR 생성 실패 시 빈 QR 대신 문자 접속정보와 오류를 보여 준다.

## 7. Mobile web UX/UI

- 기준 viewport: 360×800, 390×844 portrait
- 상단: `HMI 웹 제어`, 연결 상태 badge와 재연결 banner
- 본문: 네 switch card. 390 px 미만은 1열, 충분한 폭에서는 2열
- card 전체를 touch할 수 있고 switch label과 현재 상태를 함께 읽을 수 있게 한다.
- offline/reconnecting 중 HMI state를 추측해 바꾸지 않는다.
- reconnect backoff는 0.5, 1, 2, 5초로 제한하고 성공 시 초기 snapshot을 다시 받는다.
- reconnect 후 local optimistic state를 버리고 authoritative snapshot을 사용한다.
- HMI locale을 session locale로 사용하고 웹에 별도 locale selector를 만들지 않는다.
- 외부 font, CDN, analytics, cookie와 local storage를 사용하지 않는다.

## 8. 화면 상태와 오류 표현

| 상태 | HMI 핵심 표현 | 허용 action |
| --- | --- | --- |
| `off` | 기능 설명과 AP+STA 공존 안내 | AP 시작, Back |
| `starting` | spinner, `AP를 시작하고 있습니다.` | 중복 action 없음 |
| `waiting_wifi_client` | Wi-Fi QR, SSID와 암호 | 암호 표시, AP 종료 |
| `waiting_web_client` | URL QR, client 연결됨 | Wi-Fi QR 보기, AP 종료, HMI switch |
| `controlling` | 웹 연결됨, 네 switch 동기화 | switch, QR 전환, AP 종료 |
| `reconnecting` | web 재연결 대기, HMI state 유지 | HMI switch, AP 종료 |
| `stopping` | `AP를 종료하고 있습니다.` | 중복 action 없음 |
| `restoring_sta` | AP-only fallback의 `기존 Wi-Fi 연결을 복구하고 있습니다.` | 상태 확인 |
| `error` | 오류 문구와 복구 action | 다시 시도, 정리, Back |

오류 enum은 `radio_busy`, `ap_start`, `dhcp`, `server_start`, `qr_payload`,
`client_limit`, `protocol`, `timeout`, `stop`, `sta_restore`를 구분한다. 화면에는 raw driver
error나 secret을 표시하지 않는다.

## 9. UI text와 i18n 계약

고정 문구는 `samples/suite/i18n/ko.yaml`, `en.yaml`, `typography.yaml`이 소유한다.

- Dashboard: `page.wifi_ap_web.*`
- 제목/단계: `wifi_ap.title.*`, `wifi_ap.step.*`
- action: `wifi_ap.action.*`
- 상태: `wifi_ap.status.*`
- label: `wifi_ap.label.*`
- 안내/dialog: `wifi_ap.message.*`, `wifi_ap.dialog.*`
- 오류: `wifi_ap.error.*`
- switch: `wifi_ap.switch.1`~`wifi_ap.switch.4`

SSID, password, IP, client count와 revision은 dynamic value다. 주변 label과 format만 catalog에
둔다. 두 locale의 key와 placeholder type은 완전히 같아야 한다.

- title/body를 하나의 label에 섞지 않는다.
- 안내·오류·dialog 본문은 line spacing 5 px 이상을 명시한다.
- onboarding/QR 안내는 center, 접속정보와 오류 상세는 left align을 사용한다.
- button의 두 줄 문구는 피한다. 영문이 길면 문구를 줄이고 font를 임의 축소하지 않는다.
- 한국어/영어 light/dark에서 첫 줄과 마지막 줄 clipping을 직접 확인한다.

## 10. 상태·protocol·보안 계약

### 10.1 canonical model

`WifiApWebController`가 다음을 단독 소유한다.

- bool switch 4개
- monotonically increasing `revision`
- AP lifecycle state와 마지막 bounded error
- client count 0~2
- authenticated web client handle/bitset

HMI와 web transport는 model을 직접 수정하지 않고 command/snapshot을 사용한다.
같은 AP session의 client reconnect에서는 state와 revision을 유지하고, AP를 완전히 중지한 뒤
새 session을 시작할 때는 네 switch를 모두 `꺼짐`, revision 0으로 초기화한다.

### 10.2 secret 처리

- 일반 diagnostic snapshot은 password와 token을 포함하지 않는다.
- page가 필요할 때만 bounded `copy_access_info()`로 SSID/password/IP를 받는다.
- SSID는 `HMI-Control-XXXX` 형식으로 만들고 `XXXX`에는 장치 식별자의 마지막 4개 hex만 쓴다.
- AP password는 혼동하기 쉬운 문자를 제외한 12자 임시 값으로 session마다 CSPRNG로 만든다.
- web session token은 128-bit random 값으로 session마다 교체한다.
- Windows fixture는 고정 seed의 결정적 값만 사용하고 production secret 생성기로 재사용하지 않는다.
- password/token은 일반 log, screenshot fixture 이름과 test failure text에 남기지 않는다.
- page destroy, AP stop과 session 교체 시 secret buffer 전체를 지운다.

### 10.3 web endpoint

- `GET /`: 고정 HTML/CSS/JS bundle
- `GET /favicon.ico`: 204
- `GET /ws`: WebSocket upgrade
- 그 외 URI/method: 404/405
- WebSocket 첫 frame에서 fragment로 전달받은 session token 인증
- frame 최대 256 bytes, client 최대 2, command queue 최대 8
- JSON key/type/길이 allowlist, unknown/duplicate field 거부
- 모든 응답에 authoritative snapshot revision 포함
- `Cache-Control: no-store`, 외부 origin과 file path 입력 거부

## 11. 모듈과 책임 경계

| 위치 | 책임 |
| --- | --- |
| `modules/connectivity/include/hmi/connectivity/wifi_ap_web_*.hpp` | bounded type, port, controller와 radio ownership 계약 |
| `modules/connectivity/src/wifi_ap_web_controller.cpp` | lifecycle, canonical state, revision과 command validation |
| `modules/connectivity/tests/` | lifecycle, stale/limit/reconnect/secret 정리 test |
| `platform/windows/connectivity/virtual_wifi_ap_web.*` | PC Wi-Fi를 변경하지 않는 fixture |
| `platform/hmi/connectivity/esp_hosted_wifi_radio.*` | shared radio mode와 STA/AP view |
| `platform/hmi/connectivity/esp_http_wifi_ap_web.*` | bounded HTTP/WebSocket server와 callback/queue |
| `samples/suite/pages/wifi_ap_web/` | HMI page, web source/bundle config, QR view와 tests |
| `samples/suite/i18n/` | HMI fixed text와 typography role |
| `profiles/runtime/app_contract.hpp` | controller pointer의 host→app 전달 |
| `samples/suite/page_contract.hpp` | controller pointer의 app→page 전달 |
| `hosts/windows/main.cpp`, `hosts/hmi/main.cpp` | adapter/controller 생성, tick과 수명 조립 |
| `samples/suite/page_registry.json`, `profile.json` | page source, capability, test와 bundle 등록 |

### 11.1 기존 STA 변경의 허용 범위

physical radio 공유 때문에 completed S04의 기반 계약 일부 변경은 필요하다. 범위는 다음으로
제한한다.

- `WifiStaRegistration`에 AP 전환을 위한 suspend/resume 계약을 추가한다.
- HMI adapter는 `EspHostedWifiRadio` 한 owner가 STA/AP view를 제공한다.
- S04 page UI, credential 정책, scan 목록과 저장 형식은 변경하지 않는다.
- S04 전체 state/interaction test를 회귀 gate로 유지한다.

UI/page는 `esp_wifi`, `esp_http_server`, Arduino, FreeRTOS 또는 Board internal을 include하지
않는다. HMI host는 page ID가 아니라 generated capability로 runtime을 조립한다. Board에는
screen, web switch 또는 AP workflow 지식을 추가하지 않는다.

## 12. Web bundle과 QR source 계약

- source: `samples/suite/pages/wifi_ap_web/web/index.html`, `app.css`, `app.js`
- config: `samples/suite/pages/wifi_ap_web/web_bundle.json`
- generated: `samples/suite/pages/wifi_ap_web/generated/web_bundle.hpp`
- generic tool: `tools/assets/web/embed_web_bundle.py`

generator는 explicit config를 받고 path traversal, duplicate output, 개별/전체 size limit,
UTF-8, external URL/CDN과 stale output을 검사한다. generated header는 직접 편집하지 않는다.

## 13. 메모리와 자원 예산

| 항목 | 목표 상한 |
| --- | ---: |
| `WifiApWebSnapshot` | 128 bytes |
| switch command | 24 bytes |
| controller 객체 | 512 bytes |
| page 객체 | 4096 bytes |
| WebSocket frame | 256 bytes |
| command queue | 8개 |
| 동시 web client | 2개 |
| QR payload | 192 bytes |
| QR scratch | 1 KiB |
| embedded web bundle | 24 KiB flash |

- HTTP/WebSocket frame과 QR matrix를 task stack의 큰 local로 만들지 않는다.
- web bundle은 flash의 static bytes에서 전송하고 RAM에 전체 복사하지 않는다.
- callback은 LVGL object를 참조하지 않고 작은 command만 queue에 넣는다.
- switch snapshot 전체를 여러 owner가 복제하지 않는다.
- worker stack은 실제 high-water mark를 기록하고 단순히 늘려 해결하지 않는다.
- controller/page/core type에 `static_assert` size guard를 둔다.

## 14. 단계별 개발 계획

A0~A5는 `complete`다. 2026-11-11 사용자 완료 승인으로 S07 개발 gate를 닫았고,
출시 전 장시간·반복 실기기 회귀는 전역 G3 수용 항목으로 이관했다.

### A0. ESP-Hosted SoftAP feasibility — `complete`

**목표**

현재 P4/C6와 고정 component 조합에서 AP-only DHCP와 P4 HTTP data path가 실제로
가능한지 판정한다. stop/restart와 STA 영향은 A0.5 공존 gate로 이동한다.

**범위**

- shared radio owner의 최소 계약
- 임시 S07 diagnostic panel의 AP 시작/중지와 status
- WPA2 AP, DHCP, `/healthz`, client count
- QR, WebSocket, switch와 최종 UI는 제외

**산출물**

- version/pin/companion firmware 근거 표
- 최소 AP adapter와 health endpoint
- AP-only DHCP와 HTTP data path 기록

**자동 검증**

- version/pin static assertion이 유지된다.
- start/duplicate start/stop/restart/timeout adapter test가 통과한다.
- `python tools/project.py build --host hmi --profile sample-suite`가 통과한다.
- `python tools/project.py build --host hmi --profile main`이 통과한다.

**수동 검증**

- 사용자가 직접 upload한 장치에서 휴대폰이 AP에 연결되고 IP를 받는다.
- `http://<ap-ip>/healthz`가 열린다.

**종료 조건**

- AP DHCP와 HTTP data path의 실기기 통과. 실패하면 A0를 `blocked`로 둔다.

**의존**

- S06 V4 complete, S06 V5 유보에 대한 2026-11-10 사용자 승인.

#### A0 자동 구현·검증 기록 — 2026-11-10

자동 구현을 완료했고 2026-11-10 사용자 실기기 확인으로 휴대폰 DHCP와
`http://192.168.4.1/healthz` 응답이 정상임을 확인했다. AP-only 반복 전환과 STA 복구
확인은 사용자 요청에 따라 A0.5의 AP+STA 공존·반복 전환 gate로 대체해 보존한다.

| 항목 | 고정값·근거 | 판정 |
| --- | --- | --- |
| ESP-Hosted host component | `2.12.8`, `esp_hosted_wifi_common.hpp` compile-time assertion | build 통과 |
| `esp_wifi_remote` | `1.6.0`, 같은 shared header의 compile-time assertion | build 통과 |
| SDIO wiring | CLK 18, CMD 19, D0~D3 14~17, reset 54와 SDK config assertion | build 통과 |
| C6 companion image | host component가 기대하는 protocol은 2.12.8로 고정. 장치에 실제 설치된 image 호환성은 compile로 판정하지 않음 | 실기기 대기 |
| AP transport | WPA2-PSK, channel 1, 최대 client 2, Arduino DHCP, P4 `WebServer`의 `GET /healthz` | HMI compile 통과 |

- `WifiApRuntime`이 `STA cancel/drain → suspend → AP start → stop → STA resume` 순서를
  소유하고, AP 중에는 `WifiStaRegistration::tick()`이 STA driver를 poll하지 않는다.
- 일반 runtime/transport snapshot에는 SSID password가 없고, diagnostic page만 bounded
  credential copy API로 값을 읽는다. password는 log에 남기지 않으며 shutdown에서 overwrite한다.
- Windows fixture에서 start, duplicate start, stop, restart, client count, start failure,
  timeout과 저장된 STA 복구가 통과했다.
- order 70 diagnostic page는 한국어/영어, light/dark rebuild와 Back 정리를 지원한다.
  캡처는 `_artifacts/wifi-ap-web/a0-ready-ko-light.png`에 있다.
- typography stale check와 생성기/manifest 관련 Python test 43개가 통과했다. 전체 discover는
  system Python에는 `pyserial`이 없고 PlatformIO Python 3.11은 기존 S06 `font_assets.py`의
  Python 3.12+ f-string 문법을 읽지 못해 한 interpreter로 합쳐 실행하지 못했다. 두 실패 모두
  S07 변경과 무관하며, asset push test와 typography test는 각각 호환 interpreter에서 통과했다.
- Windows sample-suite 41개 test가 모두 통과했다.
- HMI sample-suite build: RAM 87,552 B / 512,000 B(17.1%), flash 1,560,360 B /
  6,553,600 B(23.8%). upload는 수행하지 않았다.
- HMI main 회귀 build: RAM 28,256 B(5.5%), flash 661,610 B(10.1%). upload는 수행하지 않았다.
- isolation check는 Windows/HMI의 main/sample-suite 네 generated input 모두 통과했다.

완료된 A0 수동 gate:

- [x] 사용자가 직접 올린 장치에서 `HMI-Control-A0`에 연결해 DHCP 주소를 받았다.
- [x] `http://192.168.4.1/healthz`가 정상 응답했다.

### A0.5. ESP-Hosted AP+STA 공존 feasibility — `complete`

**목표**

저장된 STA 연결과 IPv4를 끊지 않은 채 SoftAP와 `/healthz`를 시작·중지할 수 있는지
현재 P4/C6와 고정 component 조합에서 판정한다.

**범위**

- AP runtime의 `STA suspend`와 `STA keep` 정책 분리
- HMI adapter의 `WIFI_AP_STA` 시작과 AP만 종료하는 mode 복귀
- 진단 화면의 STA IPv4 유지 상태 표시
- start/stop/back 반복과 기존 STA 연결 연속성
- QR, WebSocket, switch와 최종 UX 비동기화는 제외

**산출물**

- AP+STA 공존 runtime 정책과 AP adapter
- Windows에서 STA 연결·IPv4가 AP 수명 동안 유지되는 direct test
- A0.5 진단 문구와 STA IPv4 행
- AP-only fallback 정책 보존

**자동 검증**

- suspend 정책의 기존 STA 복구 test와 keep 정책의 STA 무중단 test가 모두 통과한다.
- AP start/stop/restart, failure와 timeout direct test가 통과한다.
- `python tools/project.py check --host windows --profile sample-suite`가 통과한다.
- `python tools/project.py check --scope isolation`이 통과한다.
- `python tools/project.py build --host hmi --profile sample-suite`가 통과한다.
- `python tools/project.py build --host hmi --profile main`이 통과한다.

**수동 검증 — 2026-11-10 사용자 확인 완료**

- [x] S04에서 기존 STA가 연결되고 IPv4가 있는 상태로 S07에 진입했다.
- [x] AP 시작 전후에 진단 화면의 STA IPv4가 유지되고 복구 상태로 전환되지 않았다.
- [x] 휴대폰이 `HMI-Control-A0`의 DHCP 주소를 받고 AP 주소의 `/healthz`에 접근했다.
- [x] AP 실행 중 원래 LAN의 다른 기기에서 HMI에 접근했다.
- [x] AP 종료 뒤에도 같은 STA 연결이 유지되고 재접속 대기가 없었다.
- [x] AP 시작·종료와 재연결을 반복해도 AP와 STA 양쪽이 계속 동작했다.

**종료 조건**

- AP와 STA 양쪽 data path가 같은 session에서 동작하고 AP start/stop이 기존 STA 연결을
  끊지 않는다는 사용자 실기기 확인이 기록된다.

**의존**

- A0 complete.

#### A0.5 자동 구현·검증 기록 — 2026-11-10

- `WifiApStationPolicy`로 `suspend_and_restore` fallback과 `keep_connected` 실험 정책을
  분리했다. HMI와 Windows S07 조립은 keep 정책을 사용하고 기존 suspend 회귀 test는 유지한다.
- keep 정책은 STA의 cancel, disconnect, suspend와 reconnect를 호출하지 않는다. runtime
  snapshot과 진단 화면에 기존 STA IPv4를 표시해 AP 실행 전후 값을 직접 비교할 수 있다.
- HMI AP adapter는 시작 시 `WIFI_AP_STA`, 종료 시 credential을 지운 뒤 `WIFI_STA`로
  돌아간다. AP 시작 실패 경로도 STA mode로 정리한다.
- 새 STA 무중단 direct test와 기존 suspend/restore, start/stop/restart/failure/timeout을 포함한
  Windows sample-suite 41개 test가 fresh build 뒤 모두 통과했다.
- 한국어 light A0.5 ready 화면은 `_artifacts/wifi-ap-web/a05-ready-ko-light.png`에서
  STA IPv4 행, 안내 multiline과 action이 1024×600 안에 들어오는 것을 확인했다.
- typography catalog stale check와 네 generated input의 isolation check가 통과했다.
- HMI sample-suite build는 RAM 87,552 B / 512,000 B(17.1%), flash 1,560,780 B /
  6,553,600 B(23.8%)다. A0 대비 RAM은 같고 flash는 420 B 증가했다.
- HMI main 회귀 build는 RAM 28,256 B(5.5%), flash 661,610 B(10.1%)로 통과했다.
- Codex는 upload를 수행하지 않았다. 사용자가 같은 firmware를 직접 올려 휴대폰 AP 경로,
  원래 LAN 경로, AP 종료와 반복 재연결을 모두 확인했다. A0.5는 `complete`다.

### A1. Radio ownership와 canonical 4-switch model — `complete`

**목표**

STA/AP가 하나의 radio를 충돌 없이 사용하고 네 switch 상태가 단일 controller에서 결정되게 한다.

**범위**

- AP+STA mode lease와 AP-only suspend/resume fallback
- lifecycle state/error/snapshot
- 4-switch command/revision/stale 정책
- Windows fixture
- server, QR와 LVGL widget은 제외

**산출물**

- `WifiApWebController`, radio coordinator와 port/type headers
- Windows fixture와 controller direct tests
- secret-free diagnostic snapshot

**자동 검증**

- duplicate start/stop, owner≤1과 STA restore test가 통과한다.
- switch ID/value/revision/stale/duplicate command test가 통과한다.
- object size guard와 secret clear test가 통과한다.

**수동 검증**

- state 이름과 UI에 노출할 error grouping을 검토한다.

**종료 조건**

- radio owner≤1과 canonical state 계약 통과.

**의존**

- A0.5 complete.

#### A1 구현·검증 기록 — 2026-11-10

- `WifiApRuntime`을 host당 하나만 구성하는 radio coordinator로 명시하고, AP+STA
  `keep_connected` 기본 정책과 AP-only `suspend_and_restore` fallback을 그대로 보존했다.
- `WifiApWebController`가 AP lifecycle 진입점과 네 switch의 canonical state를 제공한다.
  snapshot은 runtime state/error/client count와 네 bool, `revision`만 포함하며 credential이나
  session secret을 포함하지 않는다.
- switch ID는 protocol과 같은 1~4로 고정했다. AP가 ready가 아니거나 ID가 범위를 벗어나면
  거부하고, 기대 revision 불일치는 state가 이미 같아도 stale로 응답한다. 현재 revision에서
  같은 값을 다시 요청하면 `unchanged` no-op으로 처리해 revision을 소비하지 않는다.
- AP가 완전히 중지된 동안에는 마지막 state/revision을 보존하고, 유효한 새 AP session 시작을
  받아들인 시점에만 네 switch를 모두 끄고 revision 0으로 초기화한다. 거부된 시작은 state를
  초기화하지 않는다.
- command/result/snapshot/controller는 고정 크기이고 각각 12/12/24/24 B 이하 static guard를
  둔다. runtime의 password는 stop 완료와 shutdown에서 overwrite하며 direct test로 빈 값을
  확인했다.
- 기존 runtime test와 새 controller direct test가 duplicate start/stop, STA restore,
  AP+STA coexistence, switch ID/value, applied/unchanged/stale, 새 session reset과 secret clear를
  검증한다. Windows 전체 45개 test와 profile 지정 42개 test가 모두 통과했다.
- generator/typography Python test 54개와 Windows/HMI main/sample-suite 네 generated input의
  isolation check가 통과했다. A0.5에서 추가된 여섯 catalog role의 stale count도 262로 맞췄다.
- HMI sample-suite build는 RAM 87,616 B / 512,000 B(17.1%), flash 1,562,726 B /
  6,553,600 B(23.8%)로 통과했다. A0.5 대비 RAM은 64 B, flash는 1,946 B 증가했다.
- lifecycle state 이름은 `off/preparing/starting/ready/stopping/restoring_sta/failed`로 유지하고,
  UI error는 invalid config, radio busy, AP start, transition timeout, STA restore 다섯 사용자
  범주로 묶는 현재 mapping이 적절한지 검토했다. server, QR와 LVGL switch는 A2/A3 범위다.
- firmware upload는 수행하지 않았다. A1에는 추가 실기기 gate가 없으며 A2는 아직 시작하지 않았다.

### A2. Bounded web server, protocol과 QR — `complete`

**목표**

외부 자산 없이 mobile browser가 인증·초기 snapshot·양방향 update·reconnect를 수행한다.

**범위**

- web bundle generator
- HTTP route, WebSocket protocol과 session token
- client/frame/queue limit
- Wi-Fi/URL QR payload와 renderer
- Windows transport/protocol fixture
- 최종 HMI layout은 제외

**산출물**

- page-owned HTML/CSS/JS와 generated bundle
- HMI HTTP/WebSocket adapter
- QR encoder/view와 provenance/license
- protocol/browser/QR tests

현재 A0 진단 page와 host가 이미 AP runtime을 조립하므로, A2의 mobile browser 자동 검증에
필요한 controller pointer와 web service의 최소 host/app 전달까지 이 단계에서 연결한다.
Dashboard registry 회귀, S04 왕복과 전체 destroy/isolation 정리는 A4 범위에 남긴다.

**자동 검증**

- bundle stale/path/external URL/size 검사가 통과한다.
- unknown method/path/key와 unauthenticated/oversized/malformed frame이 거부된다.
- reconnect가 최신 full snapshot으로 복구된다.
- Wi-Fi QR과 fragment-token URL QR payload test가 통과한다.

**수동 검증**

- 360×800과 390×844 viewport에서 clipping, horizontal scroll과 작은 touch target이 없다.
- offline/reconnecting/connected가 색상 없이 구분된다.

**종료 조건**

- browser bundle의 viewport와 reconnect 상태 표현, controller/transport의 네 상태 변경과
  reconnect full snapshot이 각각 통과한다. 실제 휴대폰과 실기기 socket의 결합 수용은 A5에서 한다.

**의존**

- A1 complete.

#### A2 구현·검증 기록 — 2026-11-10

- page-owned HTML/CSS/JS와 deterministic generator를 추가했다. config path escape, duplicate,
  UTF-8, external URL/CDN, source/24 KiB bundle 상한과 stale output을 Python test 4개로 검증했다.
- controller가 128-bit session token, 12자 임시 AP 암호, 최대 client 2, 256 B frame과 8개
  command queue를 고정 크기로 소유한다. 인증 첫 frame, unknown/duplicate/missing field, overflow,
  stale revision, reconnect full snapshot과 secret-free snapshot을 direct test로 검증했다.
- HMI AP adapter를 고정 route raw HTTP/WebSocket server로 전환했다. `/`, `/favicon.ico`,
  `/healthz`, `/ws`만 허용하고 browser frame mask/길이, token 인증, authoritative broadcast와
  AP stop 시 client close를 처리한다. ESP32-P4 실제 framework compile이 통과했다.
- Wi-Fi QR과 `#token=` URL fragment payload를 192 B 고정 버퍼로 만들고 escape, invalid input,
  작은 output buffer와 token 비노출 query를 test했다. LVGL/Nayuki provenance는 page에 기록했다.
- mobile web은 외부 자산 없이 system light/dark, text+mark 연결 상태, 0.5/1/2/5초 reconnect
  backoff와 reconnect snapshot overwrite를 사용한다. 실제 browser에서 360×800은 1열,
  390×844는 2열로 렌더링했고 가로 scroll이 없으며 card touch target은 최소 104 px다.
- Windows QR/interaction과 controller/protocol test가 통과했다. HMI sample-suite build는 RAM
  87,824 B / 512,000 B(17.2%), flash 1,567,644 B / 6,553,600 B(23.9%)다. upload는 수행하지 않았다.

### A3. HMI UX/UI와 visual acceptance — `complete`

**목표**

1024×600 흐름을 구현하고 사용자가 설명 없이 AP 연결과 웹 진입을 완료할 수 있게 한다.

**범위**

- off/starting/two QR steps/control/reconnect/error/stop UI
- 2열 layout과 네 switch card
- 종료 confirm dialog
- 한국어/영어, light/dark와 rebuild
- Windows interaction와 screenshot

**산출물**

- `WifiApWebPage`와 state/interaction tests
- catalog/typography keys와 generated result
- `_artifacts/wifi-ap-web/`의 대표 capture

**자동 검증**

- 한 touch가 command 한 번만 만든다.
- Back confirm/cancel/stop navigation과 destructor fallback test가 통과한다.
- locale/theme rebuild 뒤 AP/client/switch revision이 유지된다.
- destroy 뒤 timer, callback, QR buffer와 font handle이 남지 않는다.
- `python tools/assets/typography/generate.py --config samples/suite/i18n/catalog.json --check`가 통과한다.
- `python tools/project.py check --host windows --profile sample-suite`가 통과한다.

**수동 검증**

- 한국어 light의 off/wait/control/error와 영어 dark의 control/reconnect capture를 확인한다.
- QR, 접속정보, 네 switch와 AP 종료가 scroll 없이 모두 보인다.
- line spacing, title/body hierarchy, touch target과 danger action 구분을 확인한다.

**종료 조건**

- 두 locale·두 theme 핵심 상태가 clipping 없이 읽히고 touch flow가 중복 없이 완료된다.

**의존**

- A2 complete.

#### A3 구현·검증 기록 — 2026-11-10

- 1024×600 화면을 off/working/error의 중앙 onboarding card와 ready 상태의 360 px 연결 안내 +
  가변 제어 영역으로 구성했다. ready에서는 220×220 QR frame, 접속 정보, 2×2 switch card와
  danger 계층의 `AP 종료`가 scroll 없이 한 화면에 들어간다.
- Wi-Fi 연결 전에는 Wi-Fi QR, client 연결 후에는 token fragment를 포함한 URL QR을 표시하며
  `Wi-Fi QR 다시 보기`로 되돌아갈 수 있다. reconnect는 기존 네 switch 상태를 유지하면서
  상태 문구와 badge로 구분한다.
- 한 switch touch가 revision을 정확히 한 번만 증가시키고, reconnect snapshot, locale/theme
  rebuild, confirm cancel/stop/back, app destroy fallback을 interaction test로 검증했다.
- 한국어 light의 off/wait/control/error와 영어 dark의 control/reconnect를
  `_artifacts/wifi-ap-web/`에 capture했다. QR quiet zone, title/body hierarchy, touch target,
  danger action, clipping과 scroll 부재를 capture로 확인했다.
- catalog는 한국어·영어 283개 문구가 stale 없이 생성됐고 Windows 전체 46개 test와
  profile check 43개 test가 통과했다. HMI sample-suite build는 RAM 87,832 B / 512,000 B(17.2%),
  flash 1,585,528 B / 6,553,600 B(24.2%)다. upload는 수행하지 않았다.

### A4. Registry·host 조립과 lifecycle 통합 — `complete`

**목표**

S07을 generated registry로 조립하고 host/page 분기 없이 전체 lifecycle을 연결한다.

**범위**

- page registry/profile/capability/test 등록
- AppContext/PageContext 전달
- Windows/HMI host composition과 tick
- app destroy, S04 왕복과 isolation

**산출물**

- order 70의 S07 descriptor
- host별 adapter/controller 조립
- integration/navigation/destruction tests

**자동 검증**

- `python tools/project.py check --host windows --profile sample-suite`가 통과한다.
- `python tools/project.py check --scope isolation`이 통과한다.
- `python tools/project.py build --host hmi --profile sample-suite`가 통과한다.
- `python tools/project.py build --host hmi --profile main`이 통과한다.
- Dashboard/host에 page ID 분기가 없고 UI/module에 platform header가 없음을 검색한다.
- app destroy 뒤 AP/server/client/queue/callback/mode lease가 남지 않는다.

**수동 검증**

- Windows Dashboard에서 S07 진입·종료·재진입과 S04 왕복을 확인한다.

**종료 조건**

- registry 추가만으로 page가 나타나며 main/기존 page에 S07 정책이 들어가지 않는다.

**의존**

- A3 complete.

#### A4 구현·검증 기록 — 2026-11-11

- order 70 descriptor, page capability/test 선언과 Windows/HMI host의 AP runtime/controller 조립을
  generated registry 경로로 연결했다. Dashboard와 host에는 page ID 분기를 추가하지 않았다.
- 공용 `AppContext`/`PageContext`는 페이지 이름 대신 `ap_control` capability를 전달해 완료된
  sample composition이 S07 data key에 직접 결합되지 않게 했다.
- interaction test가 Dashboard 진입·AP 시작·locale/theme rebuild·Back cancel/stop·재진입과
  active page 상태의 app destroy를 검증한다. screen 직속 confirm layer의 1024×600 좌표와
  page destroy 시 외부 layer 정리도 함께 검사한다.
- Windows sample-suite profile test 43개, Wi-Fi AP/WAV 관련 Python test 32개와
  Windows/HMI main/sample-suite 네 생성 입력 isolation 검사가 통과했다.
- HMI sample-suite build는 RAM 87,832 B / 512,000 B(17.2%), flash 1,585,542 B /
  6,553,600 B(24.2%)로 통과했다. HMI main 회귀 build도 RAM 28,256 B(5.5%),
  flash 661,610 B(10.1%)로 통과했다. upload는 수행하지 않았다.

### A5. HMI 성능·보안·실기기 수용 — `complete`

**목표**

실제 장치와 휴대폰에서 전체 흐름, 장시간 안정성, 자원 예산과 보안 제한을 확인한다.

**범위**

- 최종 build/memory/stack/flash 근거
- Android 또는 iOS 실제 browser
- AP/STA 반복 전환, reconnect, locale/theme와 page lifecycle
- README와 최종 승인

**산출물**

- RAM/flash delta와 object/queue/bundle/worker stack 기록
- 실기기 수용 결과
- 사용법·제한·보안 주의 문서

**자동 검증**

- A0~A4 direct tests와 최종 Windows/HMI/isolation gate가 다시 통과한다.
- size guard와 web bundle limit이 유지된다.
- password/token log, 임의 endpoint, external asset과 unbounded frame 검색이 0건이다.
- `git diff --check`가 통과한다.

**수동 검증**

- A0에서 휴대폰 DHCP와 `/healthz`, A0.5에서 AP+STA 양쪽 data path와 반복 시작·종료를 확인한다.
- Windows 실제 browser에서 두 mobile viewport, reconnect와 light/dark를 확인한다.
- 1024×600 capture에서 두 QR 단계, control/reconnect/error와 전체 화면 종료 dialog를 확인한다.
- 2026-11-11 사용자가 현재 개발 범위의 S07 완료를 명시적으로 승인한다.
- 최종 firmware의 휴대폰 QR/WebSocket 장시간·반복 회귀는 기능 구현 완료와 분리해 전역 G3
  출시 수용 및 `README.md` 7절 체크리스트로 이관한다.

**종료 조건**

- 최종 자동 gate, A0/A0.5 실기기 기반 판정과 사용자의 S07 개발 완료 승인이 기록된다.
- 출시 전 반복 실기기 확인 항목이 G3와 README에 보존된다.

**의존**

- A4 complete, 사용자 직접 upload.

## 15. 단계 게이트 요약

| 단계 | 상태 | 핵심 종료 조건 | 다음 단계 허용 |
| --- | --- | --- | --- |
| A0 | `complete` | 실제 AP DHCP와 `/healthz` | A0.5 |
| A0.5 | `complete` | STA 무중단 AP start/stop과 양쪽 data path | A1 |
| A1 | `complete` | radio owner≤1과 canonical 4-switch model | A2 |
| A2 | `complete` | bounded web/QR와 mobile reconnect | A3 |
| A3 | `complete` | 1024×600 UX/UI visual acceptance | A4 |
| A4 | `complete` | registry/host/lifecycle/isolation 통합 | A5 |
| A5 | `complete` | 최종 자동 gate·기반 실기기 근거·사용자 승인 | S07 complete |

A0~A5 gate를 순서대로 통과했고 2026-11-11 사용자 승인으로 S07을 `complete`로 닫았다.

## 16. 후속 변경 격리 확인

- switch label 변경: page i18n/web source와 page config만 변경한다.
- switch 개수 가변화: 별도 계획으로 model/protocol/UI를 함께 변경한다.
- AP/STA 정책 변경: radio coordinator와 HMI adapter만 변경하고 page/web protocol은 유지한다.
- 새 board 추가: 새 HMI AP/web adapter를 구현하고 controller/page/web bundle은 유지한다.
- 제품 profile 재사용: 제품 page가 같은 controller를 조립하되 sample 문구를 module로 옮기지 않는다.
- captive portal 추가: 별도 DNS/OS 호환성 범위로 계획하고 URL QR 기본 경로는 유지한다.

이 변형이 S04 page, Dashboard 또는 host의 page ID 분기를 반복해서 요구하면 조립 경계를
잘못 잡은 것으로 보고 다음 단계 전에 수정한다.

## Automated Verification Checklist

- [x] S06 V4가 `complete`이고 V5 유보·S07 우선 진행에 대한 사용자 승인이 기록돼 있다.
- [x] 현재 ESP-Hosted 조합에서 AP DHCP/HTTP가 실기기 근거와 함께 판정됐다.
- [x] AP+STA 공존 start/stop/restart와 양쪽 data path가 실기기 근거와 함께 판정됐다.
- [x] STA/AP mode owner가 동시에 둘 이상 존재하지 않는다.
- [x] 4-switch command/snapshot/revision/stale와 session reset tests가 통과한다.
- [x] password가 일반 snapshot/log에 포함되지 않고 stop/shutdown에서 정리된다.
- [x] HTTP method/path와 WebSocket auth/frame/client/queue limit tests가 통과한다.
- [x] web bundle stale/path/size/external dependency 검사가 통과한다.
- [x] Wi-Fi QR과 URL QR payload/quiet-zone/render failure tests가 통과한다.
- [x] locale/theme rebuild 뒤 AP/client/switch state가 유지된다.
- [x] Back confirm과 page/app destroy 뒤 AP/server/callback/lease가 남지 않는다.
- [x] 기존 S04 Wi-Fi state/interaction tests가 통과한다.
- [x] `python tools/assets/typography/generate.py --config samples/suite/i18n/catalog.json --check`가 통과한다.
- [x] `python tools/project.py check --host windows --profile sample-suite`가 통과한다.
- [x] `python tools/project.py check --scope isolation`이 통과한다.
- [x] `python tools/project.py build --host hmi --profile sample-suite`가 통과한다.
- [x] `python tools/project.py build --host hmi --profile main`이 통과한다.
- [x] 객체, QR buffer, web bundle, queue, worker stack과 RAM/flash 변화가 기록된다.
- [x] `README.md`에 AP 연결, 두 QR, AP+STA 유지, 제한과 보안 주의가 기록된다.
- [x] `git diff --check`가 통과한다.

## G3 출시 수용 이관 체크리스트

아래 미실행 항목은 2026-11-11 사용자 범위 결정에 따라 S07 개발 완료를 막지 않으며,
출시 전 전역 G3와 `README.md` 7절에서 다시 확인한다. 기존 항목을 삭제하지 않고 그대로
보존한다.

- [x] AP 꺼짐 화면에서 목적, AP+STA 영향과 첫 action이 즉시 이해된다.
- [ ] Wi-Fi QR은 휴대폰으로 안정적으로 인식되고 문자 SSID/암호도 읽을 수 있다.
- [ ] client 연결 후 URL QR로 자동 전환되며 이전 QR로 돌아갈 수 있다.
- [ ] URL QR이 외부 인터넷 없이 local web을 연다.
- [x] mobile web 360×800과 390×844에서 horizontal scroll과 잘린 문구가 없다.
- [ ] HMI와 웹의 네 switch가 양방향으로 300 ms 목표 안에 동기화된다.
- [ ] browser 재연결 뒤 네 상태가 authoritative snapshot과 일치한다.
- [x] AP 종료와 Back confirm이 종료 영향을 명확히 알리고 중복 실행되지 않는다.
- [x] AP 시작·종료와 반복 재연결 중 기존 STA와 IPv4가 유지된다.
- [x] 한국어/영어와 light/dark Windows capture에서 QR, 접속정보, switch, 상태와 dialog가 잘리지 않는다.
- [ ] multiline spacing, title/body hierarchy와 52 px 이상 touch target이 실제 LCD에서 안정적이다.
- [x] locale/theme 전환은 AP/client/switch state를 재시작하지 않는다.
- [ ] AP 시작·종료와 page 왕복 10회 뒤 reset, 멈춤, 중복 callback과 radio 충돌이 없다.
- [ ] serial log에 password/token, watchdog, stack overflow와 allocation 실패가 없다.
- [x] 사용자가 S07 완료를 명시적으로 승인한다. (2026-11-11)

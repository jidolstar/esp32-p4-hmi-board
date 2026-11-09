# S08 계획: Wi-Fi AP & Web Control Page

작성: 2026-11-08. 상태: `pending`. 선행 조건: S07 `camera-gallery` complete.

## 1. 목표와 범위

HMI에서 AP mode를 켜고 SSID, 암호, IP와 접속 절차를 표시한다. 휴대폰이 AP에 접속한 뒤
QR로 로컬 웹페이지를 열어 4개의 고정 switch를 조작하면 HMI 화면의 동일한 4개 상태가
즉시 갱신된다. HMI에서 바꾼 상태도 웹에 반영한다.

이 기능은 202609의 STA 설정에 없던 신규 확장이다. ESP-Hosted의 SoftAP/AP+STA 지원을
사용하되 현재 저장소의 실제 component/co-processor 버전으로 먼저 검증한다.
공식 [ESP-Hosted MCU AP+STA 예제](https://github.com/espressif/esp-hosted-mcu/blob/main/examples/wifi/apsta/README.md)는
ESP32-P4 host와 ESP32-C 계열 co-processor 조합을 지원 대상으로 명시하지만, 이 사실이 현재
고정 버전의 firmware 조합까지 자동으로 보증하지는 않으므로 A0 probe를 생략하지 않는다.

## 2. 사용자 흐름과 통신 계약

1. `AP 시작`을 누르면 WPA2 이상 임시 암호와 SSID를 표시한다.
2. 연결 전에는 Wi-Fi 접속 QR과 문자 정보를 보여준다.
3. 휴대폰 접속 후에는 `http://<device-ip>/` URL QR을 보여준다. captive portal 자동 열기는
   OS별 보조 기능일 뿐 완료 조건으로 삼지 않는다.
4. 웹은 고정 switch 4개와 연결 상태만 제공한다.
5. 각 변경은 switch ID, bool value, revision을 보내고 HMI의 canonical state가 응답한다.
6. WebSocket을 우선 사용하고 연결/재연결/초기 snapshot을 명시한다. 임의 명령, 파일 접근,
   firmware update, camera/gallery 접근은 제공하지 않는다.

## 3. 안전/자원 계약

- AP mode와 기존 STA 연결의 관계를 명시한다. 1차 구현은 AP-only 전환을 기본으로 하고,
  AP+STA는 같은 버전 조합에서 검증될 때만 활성화한다.
- SSID/암호 길이, client 수, HTTP header/body, WebSocket frame, queue를 제한한다.
- AP 암호는 화면 표시가 필요하지만 일반 log와 diagnostic snapshot에는 남기지 않는다.
- 웹 자산은 작은 고정 bundle이며 SD에 의존하지 않는다.
- state owner는 HMI 공통 model 하나이며 웹과 LVGL은 command/snapshot으로만 접근한다.

## 4. 단계

### A0. Hosted AP feasibility gate

현재 ESP-Hosted/wifi-remote/co-processor 조합에서 SoftAP data path, DHCP, socket server와
AP stop/restart를 최소 probe로 검증한다. STA→AP mode 전환과 복귀 정책을 확정한다. phone이
AP에 연결하고 HMI의 HTTP endpoint에 접근해야 A1로 간다. RPC만 존재하고 data path가
동작하지 않으면 `blocked`로 두고 component/companion firmware 갱신을 별도 결정한다.

### A1. AP와 canonical switch model

AP lifecycle port, 4-switch bounded model, revision/command/result, client connect 상태를 구현한다.
Windows mock에서 stale revision, duplicate command, reconnect, AP restart를 test한다. 상태가
항상 HMI model 하나에서 결정됨을 확인해야 A2로 간다.

### A2. web server와 QR

고정 HTML/CSS/JS bundle, WebSocket endpoint, 초기 snapshot과 update broadcast를 구현한다.
Wi-Fi QR와 URL QR을 각각 생성하고 잘못된 입력/초과 frame을 거부한다. browser 자동 test와
payload/security limit 검사가 통과해야 A3로 간다.

### A3. HMI page와 실기기 수용

AP 시작/종료, 접속정보, QR, client 수, HMI switch 4개, 동기화 상태를 표시한다. page 이탈
정책에 따라 AP/server를 명시적으로 종료하고 callback을 정리한다. 사용자가 Android/iOS
또는 휴대폰 1대에서 연결·QR·양방향 갱신·재연결을 확인하면 S08을 완료한다.

### 단계 게이트 요약

| 단계 | 목표·범위 | 산출물 | 자동 확인 | 수동 확인 | 종료 조건·의존 |
| --- | --- | --- | --- | --- | --- |
| A0 | 현재 Hosted 조합의 SoftAP data path 판정 | version 표, AP probe, STA/AP 정책 | component/config/compile/network probe 검사 | 휴대폰 AP 연결과 HTTP 접근 | 실제 data path 성공. S07 complete |
| A1 | AP lifecycle과 canonical 4-switch state | AP port, state/revision model, mock | lifecycle/stale/duplicate/reconnect test | 상태 owner/UX 검토 | canonical state 계약 통과. A0 complete |
| A2 | local web/QR/protocol 구현 | 고정 web bundle, WebSocket, 두 QR | browser/payload/limit/security test | 휴대폰 browser 흐름 사전 확인 | 외부 의존 없이 동기화. A1 complete |
| A3 | HMI UI와 실제 양방향 수용 | 접속정보/QR/switch UI, build/memory 기록 | Windows/HMI build, isolation | QR 접속·양방향 4-switch·재연결·STA 복귀 | 사용자 승인. A2 complete |

## Automated Verification Checklist

- [ ] 현재 Hosted 버전의 SoftAP data path와 AP lifecycle 근거가 기록된다.
- [ ] 4-switch command/snapshot/revision test가 통과한다.
- [ ] duplicate/stale/out-of-range/oversized request가 거부된다.
- [ ] WebSocket reconnect가 canonical snapshot으로 복구된다.
- [ ] Wi-Fi QR과 URL QR payload test가 통과한다.
- [ ] 웹 bundle에 외부 CDN·임의 command/file endpoint가 없다.
- [ ] Windows browser test, HMI build와 isolation check가 통과한다.
- [ ] client/frame/queue/web bundle/worker stack과 firmware size가 기록된다.

## Manual Verification Checklist

- [ ] AP 접속정보와 Wi-Fi QR로 휴대폰이 연결된다.
- [ ] URL QR로 local web page가 열린다.
- [ ] 웹 4개 switch 변경이 HMI에 즉시 반영된다.
- [ ] HMI switch 변경이 웹에 즉시 반영된다.
- [ ] 휴대폰 재연결과 AP 재시작 후 4개 상태가 일치한다.
- [ ] AP 종료 후 STA 설정 page로 돌아가 정상 연결할 수 있다.

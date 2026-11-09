# S04 계획: Wi-Fi 설정 Page

작성: 2026-11-08. 상태: `complete` (W0~W3 `complete`).
선행 조건: S03 `display-audio` complete.

## 1. 목표와 범위

Wi-Fi STA 검색, SSID 선택/직접 입력, 암호 입력, 연결·해제·재시도, IP와 신호 상태,
재부팅 후 허용된 연결 복원을 제공한다. 202609의 공통 상태와 ESP-Hosted STA adapter를
현재 profile 구조에 맞게 이식한다. AP 웹 제어는 S08에서 별도로 구현한다.

참조: `../dev_202609_hmi/src/connectivity/`,
`../dev_202609_hmi/src/platform/hmi/connectivity/esp_hosted_wifi_sta.*`,
`../dev_202609_hmi/src/ui/pages/connectivity/wifi_sta_screen.*`.

## 2. 핵심 계약

- 상태는 idle/scanning/connecting/connected/disconnected/failed로 제한한다.
- scan/connect마다 generation을 두어 늦은 결과가 현재 상태를 덮지 못하게 한다.
- AP 목록, SSID, 암호, IP 문자열과 event queue는 고정 상한을 둔다.
- 암호는 log, diagnostic snapshot, UI 오류, 일반 state copy에 포함하지 않는다.
- Windows는 가상 AP fixture를 사용하고 실제 PC Wi-Fi 설정을 변경하지 않는다.
- HMI callback은 UI 객체를 직접 참조하지 않고 작은 snapshot만 전달한다.

## 3. 단계

### W0. 이식/버전 확인

현재 ESP-Hosted·wifi-remote 버전, C6 firmware 대응, pin/transport, NVS credential schema를
확정한다. 202609 adapter의 제품 profile 의존을 제거할 목록을 만든다. host와 co-processor
버전 조합이 확인되고 contract compile test가 통과해야 W1로 간다.

### W1. 상태와 adapter

공통 scan/connect model, bounded snapshot, credential store port를 이식한다. Windows fixture는
정상, 빈 검색, 인증 실패, timeout, disconnect, 늦은 응답을 제공한다. 모든 상태 test와
암호 비노출 검사가 통과해야 W2로 간다.

### W2. page UI

검색 목록, 새로고침, SSID/암호 입력, 연결/해제, IP·RSSI·실패 안내를 구현한다. page 이탈
시 scan을 취소하되 이미 성립한 시스템 연결은 정책에 따라 유지한다. 두 locale, 긴 SSID,
keyboard 연계와 destruction test가 통과해야 W3로 간다.

### W3. HMI 수용

HMI build와 queue/worker/stack/RAM을 기록한다. 사용자가 실제 AP 검색, 연결, 잘못된 암호,
끊김, 재연결, 재부팅 복원을 확인하면 S04를 `complete`로 전환한다.

### 단계 게이트 요약

| 단계 | 목표·범위 | 산출물 | 자동 확인 | 수동 확인 | 종료 조건·의존 |
| --- | --- | --- | --- | --- | --- |
| W0 | Hosted/C6/credential 이식 가능성 확인 | 버전 표, 이식 목록, port 초안 | source/version/contract compile 검사 | transport와 companion firmware 확인 | 지원 조합 확정. S03 complete |
| W1 | STA state와 adapter 이식 | state model, mock fixture, credential store | 정상/빈 목록/실패/timeout/late result/비밀 비노출 test | fixture 의미 검토 | 상태·보안 계약 통과. W0 complete |
| W2 | STA 설정 UI | scan/list/input/connect/status UI | interaction, keyboard, locale, destruction test | Windows 긴 SSID와 오류 흐름 확인 | UI 수명·layout 통과. W1 complete |
| W3 | 실제 네트워크 수용 | build/memory/실기기 기록 | Windows/HMI build, isolation | 실제 scan/connect/failure/reboot | 사용자 승인. W2 complete |

## Automated Verification Checklist

- [x] scan/connect 정상·취소·timeout·늦은 결과 test가 통과한다.
- [x] AP 목록 dedupe와 bounded snapshot test가 통과한다.
- [x] 암호가 log/snapshot/test failure에 나타나지 않는다.
- [x] Windows mock은 실제 네트워크 설정을 변경하지 않는다.
- [x] page navigation/re-entry/destruction와 두 locale 검사가 통과한다.
- [x] Windows/HMI build와 isolation check가 통과한다.
- [x] UI에 ESP-Hosted/Board internal header가 없다.
- [x] queue, credential, worker stack과 firmware size가 기록된다.

## 4. 실행 근거

### W0 `complete`

- 현재와 이식 원본은 같은 PlatformIO platform URL을 사용한다. 설치된 host framework는
  Arduino-ESP32 `3.3.9`, precompiled ESP-IDF library는 `5.5.4+sha.735507283d`다.
- framework header에서 ESP-Hosted `2.12.8`, esp_wifi_remote `1.6.0`을 확인했다.
  제공된 companion image는 `esp32c6-v2.12.8.bin`이며 SHA-256은
  `38A14C593C1A11C7D242E43D46533C07D2DE3CCE26498A96DC5FBE4E06F3E110`이다.
- 대상은 ESP32-C6 co-processor와 SDIO slot 1, 4-bit, 40 MHz 조합이다. P4 pin은
  CMD 19, CLK 18, D0~D3 14~17, C6 reset 54로 현재 framework config와 일치한다.
- 202609의 공통 계약은 SSID 32 byte, password 64 byte, scan 최대 32개,
  generation 기반 late result 폐기, 링크+IP 이후 credential 저장을 사용한다. 제품 profile,
  기존 화면, 전역 `wifi-sta` namespace와 log macro는 그대로 이식하지 않는다.
- profile NVS namespace 아래 schema 1 credential record를 두고 magic/schema/checksum/readback을
  검증한다. password는 UI 입력 buffer와 단일 credential owner 외 snapshot/log에 복사하지 않는다.
- C++17 `-Wall -Wextra -Wpedantic -fsyntax-only`로 202609의 credential, scan,
  registration 공통 source 3개를 검사해 경고와 오류 없이 통과했다.
- 실제 companion firmware와 RF 동작은 upload 없이 확인할 수 없으므로 W3 실기기 gate에서
  host log의 version handshake와 AP 검색 결과를 확인한다. W0는 package·계약 조합을 확정했다.

### W1 `complete`

- `modules/connectivity/`가 bounded credential, scan/connect 상태와 store port를 소유한다.
- `platform/windows/connectivity/`는 실제 PC 네트워크를 변경하지 않는 결정적 fixture를,
  `platform/hmi/connectivity/`는 ESP-Hosted와 profile NVS adapter를 소유한다.
- W1에서는 page·catalog·font를 변경하지 않는다. W2는 W1 자동 gate가 모두 통과한 뒤 시작한다.
- bounded core는 SSID 32 byte, password 64 byte, AP 32개 상한과 generation 기반
  늦은 결과 폐기를 구현한다. 같은 SSID/security는 가장 강한 RSSI 한 항목만 남긴다.
- Windows fixture는 실제 PC network를 사용하지 않고 정상, 빈 검색, scan 실패, 인증 실패,
  연결 timeout, 연결 후 끊김을 poll 횟수로 재현한다.
- `hmi_connectivity_wifi_sta` test가 정상 연결·IP 후 저장, 빈 목록, 중복 제거, stale 선택,
  cancel 뒤 late result drain, 인증 실패, timeout, 끊김, 저장 실패·재시도를 통과했다.
  공개 snapshot raw byte에도 fixture password가 없음을 검사했다.
- HMI adapter는 ESP-Hosted/esp_wifi_remote version과 SDIO pin을 compile-time 검사한다.
  scan/RSSI worker는 각각 4,096 byte stack을 사용하고 AP batch는 실제 개수만 heap에 둔다.
  `WifiScanEntry`는 48 byte 이하, snapshot은 16 byte 이하, registration은 256 byte 이하다.
- Windows `sample-suite` 24/24, `main` 2/2, Python 47/47과 네 profile isolation이
  통과했다. HMI compile-only는 `sample-suite` RAM 36,172 byte(7.1%), flash
  1,360,694 byte(20.8%), `main` RAM 28,224 byte(5.5%), flash 661,450 byte(10.1%)로
  성공했다. firmware upload는 수행하지 않았다.

### W2 `complete`

- `samples/suite/pages/wifi/`가 목록·입력·연결 상태와 page-local UI lifetime을 소유한다.
- suite catalog는 고정 label·action·status·error 문구만 소유하고 SSID/IP/RSSI는 동적 값으로
  format한다. locale과 keyboard mode는 독립 상태로 유지한다.
- page 이탈 시 진행 중 scan만 cancel하고, 저장된 system connection과 host runtime은 유지한다.
- 202609의 `render_list()`→`render_input()` 흐름을 따라 기본 화면은 좌측 연결 상태와
  우측의 넓은 AP 목록으로, 선택 뒤 화면은 SSID 확인·자격 증명·가상 keyboard 전용 화면으로
  분리했다. 동적 SSID와 연결 상세는 `UserTextLabel`, 입력 caret는 `UserTextInputLine`을
  사용한다.
- password는 mask로만 표시하고 connect 요청 직후와 destroy에서 backing model 전체를
  명시적으로 지운다. page snapshot/debug API에도 password를 반환하지 않는다.
- state test는 4개 정규화 검색 결과, 32-byte SSID, locale/keyboard 독립성, 연결·IP·RSSI,
  저장, page 이탈 후 연결 유지와 검색 중 destroy/cancel을 검사한다. interaction test는
  실제 widget event로 검색·선택·입력·연결·locale 전환·Dashboard 복귀를 검사한다.
- 연결 완료 화면을 Windows screenshot으로 확인해 label/상태 겹침을 수정했고, 동적 문자열을
  고정 catalog font에서 사용자 font widget으로 옮겼다.

### W3 `complete`

- Windows `sample-suite` 26/26, `main` 2/2, Python 47/47과 네 profile isolation이
  통과했다.
- HMI compile-only는 `sample-suite` RAM 36,172 byte(7.1%), flash 1,383,166 byte(21.1%),
  `main` RAM 28,224 byte(5.5%), flash 661,450 byte(10.1%)로 성공했다.
- scan/RSSI worker stack은 각각 4,096 byte이며 AP 목록은 최대 32개를 실제 결과 수만큼
  heap에 할당한다. 공개 scan snapshot은 16 byte 이하, registration은 256 byte 이하다.
- firmware upload는 수행하지 않았다. 실제 Hosted/C6 handshake, AP 검색·인증 실패·끊김·
  재부팅 복원은 아래 수동 gate와 사용자 승인을 기다린다.
- 첫 실기기 확인에서 저장 자격 증명 자동 연결 중 누른 scan 요청이 사라지는 회귀가
  확인됐다. 202609의 `WifiStaScreen`은 `auto_scan_`을 유지해 연결 작업 종료 뒤 scan을
  시작하지만, 이식된 page는 `request_scan()`을 한 번만 호출해 busy이면 요청을 버렸다.
  연결까지 실패하면 빈 목록과 `driver_start` 오류가 함께 남아 장치 시작 오류처럼 보였다.
- page 진입 시 목록이 준비되지 않았으면 `scan_pending_`을 설정하고, 새로고침도 연결 중에는
  대기 요청으로 보관한다. host가 연결 종료를 반영한 첫 page refresh에서 실제 scan을 시작하며
  요청 수락 시 이전 연결 오류를 지운다.
- 저장 credential 자동 연결을 인증 실패로 끝낸 뒤 지연 scan이 실행되어 dedupe된 AP 4개를
  표시하고 오류가 `none`으로 복구되는 회귀 검사를 추가했다. Windows `sample-suite` 29/29,
  HMI compile-only와 네 profile isolation이 통과했다. HMI `sample-suite`는 RAM 36,172 byte
  (7.1%), flash 1,383,278 byte(21.1%)다.
- 수정 firmware upload는 수행하지 않았다. 사용자의 실제 목록·연결 재확인이 남아 W3를
  `blocked`로 유지한다.
- 실기기 UI 확인에서 기존 이식 화면이 340px 목록 옆에 입력 필드·상태·keyboard를 항상
  노출해 AP 이름, RSSI, 보안 정보와 선택 상태를 읽기 어렵게 만든 사실을 확인했다. 202609의
  검증된 두 단계 정보 구조와 대조해 목록과 입력 화면을 분리했다.
- AP 행은 SSID와 보안·RSSI를 두 줄로 표시하고, 선택 테두리와 `선택됨` 배지를 유지한다.
  지원하지 않는 보안망은 흐리게 비활성화한다. 선택하면 `선택한 네트워크`와 SSID가 표시되는
  전용 화면으로 이동하며, 직접 SSID 입력도 목록 하단의 별도 action으로 분리했다. 선택 뒤에도
  남던 `연결할 네트워크를 선택하거나 SSID를 입력하세요.` 문구는 상태별 문구로 교체했다.
- 6개 고정 행 상한도 제거했다. 최대 32개 scan 결과를 모두 세로 scroll에 표시하되 AP 최대치만큼
  widget을 미리 만들지 않고, 현재 결과 수만큼 `NetworkRow`를 checked heap에 생성한다. page의
  4,096-byte size guard와 scan의 32개 상한은 그대로 유지한다.
- 목록·선택·연결 완료 1024x600 capture에서 겹침과 잘림을 재검토했다. Windows
  `sample-suite` 29/29, catalog 132개, user font asset 176개와 네 profile isolation이
  통과했다. HMI compile-only는 RAM 36,172 byte(7.1%), flash 1,389,294 byte(21.2%)다.
  firmware upload는 수행하지 않았으며 실제 LCD 재확인 전까지 W3는 `blocked`다.
- 최초 증분 검증에서는 기존 `catalog_test.cpp` object가 재컴파일되지 않아 120개로 고정된
  `kTextCount` 계약을 놓쳤다. 이를 132개로 갱신하고 신규 Wi-Fi 문구 두 개의 locale assertion을
  추가했다. 사용자의 실행과 같은 Windows 전체 build에서 실행 파일까지 생성한 뒤 29/29 test를
  다시 통과시켰다.
- 후속 실기기 UI 의견에 따라 SSID와 보안·RSSI를 하나의 세로 text group으로 묶어 행 중앙에
  정렬하고, scan 중에는 목록 중앙 spinner와 검색 안내를 표시한다. 선택 후 암호를 입력하지 않고
  뒤로 가면 SSID·선택 상태·암호를 모두 취소해 같은 AP를 즉시 다시 선택할 수 있게 했다.
- `UserTextLabel`의 투명 root가 기본 `clickable` 상태로 AP 행 터치를 가로채던 원인을 제거했다.
  Wi-Fi action은 `CLICKED` event로 통일하고 주요 touch target을 44~48px로 키웠다. 로딩·목록·
  입력 1024x600 capture와 Windows 전체 build 및 29/29 test가 통과했다. HMI compile-only는
  RAM 36,172 byte(7.1%), flash 1,389,926 byte(21.2%)이며 firmware upload는 수행하지 않았다.
  실제 LCD의 터치와 행 가독성 재확인 전까지 W3는 `blocked`다.
- 후속 capture 재검토에서 flex로 중앙 정렬한 보안·RSSI label의 하단 glyph가 잘린 결함을
  확인했다. SSID 30px와 meta 24px를 58px text group에 명시 배치하고, test에서 meta 높이가
  실제 font line height 이상이며 group 경계를 넘지 않는지 검사한다.
- 202609의 현재 SSID 비교와 실패 재시도 계약을 복원했다. 연결 요청 수락만으로 목록으로
  이동하지 않고 입력 화면에서 `연결 중`을 표시하며, 실제 IP 확보 후에만 목록으로 돌아간다.
  인증 실패 시 password를 지운 채 같은 SSID와 입력 화면을 유지하고 정확한 오류를 표시한다.
  재입력 성공 후 scan 행은 연결 SSID를 비교해 굵은 테두리와 `연결됨` badge를 표시한다.
- 목록·연결 중·인증 실패·연결 완료 1024x600 capture를 직접 재확인했다. catalog 132개,
  Windows 전체 build 및 29/29 test, 네 profile isolation이 통과했다. HMI compile-only는
  RAM 36,172 byte(7.1%), flash 1,390,344 byte(21.2%)다. 이전 저장 네트워크 복구로 IP가
  생겨도 실패 상태를 성공으로 오인하지 않도록 완료 상태까지 함께 검사한다. firmware upload는
  수행하지 않았고 당시에는 실제 LCD 재확인 전까지 W3를 `blocked`로 유지했다.
- 2026-11-09 사용자가 Wi-Fi 개발을 완료한 것으로 처리하고 다음 page 개발을 시작하도록
  명시적으로 승인했다. 이 승인을 W3 수동 수용 gate의 종료 근거로 기록하고 S04를
  `complete`로 전환한다.

## Manual Verification Checklist

- [x] 실제 AP 목록과 RSSI가 갱신된다. (2026-11-09 사용자 완료 승인)
- [x] 정상 연결 후 IP가 표시되고 해제가 동작한다. (2026-11-09 사용자 완료 승인)
- [x] 잘못된 암호·AP 없음·timeout이 구분되어 표시된다. (2026-11-09 사용자 완료 승인)
- [x] page 왕복과 재부팅 후 허용된 연결 상태가 복원된다. (2026-11-09 사용자 완료 승인)
- [x] 반복 검색/연결 중 UI 멈춤·reset·암호 노출이 없다. (2026-11-09 사용자 완료 승인)
- [x] 검색 중 spinner가 보이고 AP 행의 두 줄이 세로 중앙에 정렬된다. (2026-11-09 사용자 완료 승인)
- [x] 선택 후 `목록으로`를 누르면 선택이 해제되고 같은 AP를 다시 선택할 수 있다. (2026-11-09 사용자 완료 승인)
- [x] AP 행·새로고침·목록으로·연결 버튼이 실제 touch에서 안정적으로 눌린다. (2026-11-09 사용자 완료 승인)
- [x] 잘못된 암호는 입력 화면에 오류를 표시하고 올바른 암호를 다시 입력할 수 있다. (2026-11-09 사용자 완료 승인)
- [x] 연결 성공 AP는 주변 네트워크 목록에서 `연결됨`으로 강조된다. (2026-11-09 사용자 완료 승인)

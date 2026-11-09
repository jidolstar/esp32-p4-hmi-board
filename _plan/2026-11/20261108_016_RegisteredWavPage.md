# S06 계획: 등록 WAV 재생 Page

작성: 2026-11-08. 상태: `pending`. 선행 조건: S05 `sd-storage` complete.

## 1. 목표와 범위

샘플에서 실제로 사용할 수 있는 여러 WAV를 asset manifest에 등록하고, page에서는 목록의
`재생`만 제공한다. 음량 조절은 S03, SD의 PC 전환은 S05가 소유한다.

- 후보 역할: 시작, 완료, 알림, 경고처럼 제품에 종속되지 않은 짧은 효과음.
- UI는 이름, 짧은 설명, 재생 button, 현재 재생/완료/오류 상태만 표시.
- 새 재생 요청은 이전 출력을 안전하게 교체한다. 별도 pause/resume/playlist/editor는 없다.
- 자산 page는 만들지 않는다. WAV manifest·hash·bundle 검사는 build/deploy 기반이다.

참조: `../dev_202609_hmi/src/hardware/audio.*`,
`../dev_202609_hmi/src/hardware/internal/wav_reader.*`,
`../dev_202609_hmi/src/diagnostics/audio_lab_catalog.*`,
`../dev_202609_hmi/assets/sounds/`.

## 2. 자산과 메모리 계약

- 사용 허가가 명확한 자체 생성/소유 음원만 포함한다.
- PCM WAV 지원 형식과 각 파일의 sample rate/channel/bit depth/size/hash를 manifest에 고정한다.
- WAV는 SD asset bundle에 배치하고 firmware RAM에 전체 로드하지 않는다.
- 단일 audio owner와 bounded chunk/ring buffer를 사용한다.
- 누락/손상/unsupported 파일은 해당 항목 오류로 표시하고 다른 음원은 계속 재생 가능하다.

## 3. 단계

### V0. 음원 선정과 이식

202609 음원의 출처/권리/용도를 확인하고 중립적인 최종 목록을 정한다. audio facade와 reader의
제품 결합을 제거하고 S03 volume service와 공유할 public audio owner를 확정한다. manifest와
header 검사 결과가 통과해야 V1로 간다.

### V1. catalog와 playback port

bounded catalog, play/replace/complete/error 상태와 SD lease 연계를 구현한다. 누락, 손상,
EOF, 빠른 교체, S05 MSC 진입 거부, 종료 후 재생 test를 통과해야 V2로 간다.

### V2. 단순 page

등록 항목별 재생 button과 현재 상태만 구현한다. page 이탈 시 정책에 따라 즉시 stop하고
callback/stream을 해제한다. 두 locale과 navigation/destruction test가 통과해야 V3로 간다.

### V3. 실기기 수용

asset bundle, Windows 상태 mock, HMI build를 확인하고 buffer/stack/RAM을 기록한다. 사용자가
모든 등록 음원의 실제 출력, 음량 연계, 빠른 교체, page 이탈, MSC 충돌 방지를 확인하면
S06을 완료한다.

### 단계 게이트 요약

| 단계 | 목표·범위 | 산출물 | 자동 확인 | 수동 확인 | 종료 조건·의존 |
| --- | --- | --- | --- | --- | --- |
| V0 | 등록 음원과 audio 이식 범위 확정 | 최종 catalog, 권리 metadata, port/manifest | WAV header/hash/source/contract 검사 | 음원 용도와 실제 출력 경로 검토 | 음원·권리·형식 확정. S05 complete |
| V1 | catalog/playback/SD lease 구현 | bounded catalog, mock/HMI adapter | play/replace/EOF/error/MSC 충돌 test | 상태 의미 검토 | 재생 수명 계약 통과. V0 complete |
| V2 | play-only UI | 항목/설명/play/status UI | locale/interaction/navigation/destruction test | Windows 목록·상태 확인 | 불필요 control 없음. V1 complete |
| V3 | asset와 실제 출력 수용 | bundle/build/memory/청취 기록 | asset check, Windows/HMI build, isolation | 전 음원·음량·교체·이탈·MSC 확인 | 사용자 승인. V2 complete |

## Automated Verification Checklist

- [ ] WAV manifest의 경로·형식·크기·hash 검사가 통과한다.
- [ ] 포함 음원의 사용 권리/출처 metadata가 존재한다.
- [ ] play/replace/EOF/error/replay state test가 통과한다.
- [ ] 전체 WAV eager load와 큰 자동 buffer가 없다.
- [ ] MSC owner 동안 재생이 거부되고 복귀 후 재생된다.
- [ ] asset bundle, Windows/HMI build와 isolation check가 통과한다.
- [ ] page 종료 후 stream/worker/output/callback이 남지 않는다.
- [ ] chunk/ring buffer, worker stack과 firmware size가 기록된다.

## Manual Verification Checklist

- [ ] 각 등록 음원의 이름과 실제 소리가 의도와 일치한다.
- [ ] 여러 재생 button을 빠르게 눌러도 소리가 겹치거나 멈추지 않는다.
- [ ] S03 음량과 mute가 모든 음원에 적용된다.
- [ ] 누락/손상 항목 뒤 정상 음원을 다시 재생할 수 있다.
- [ ] page 이탈과 MSC 전환 시 출력과 SD handle이 안전하게 정리된다.

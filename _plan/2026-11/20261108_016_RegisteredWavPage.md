# S06 계획: 등록 WAV Player Page

작성: 2026-11-08. 상세화: 2026-11-10. 상태: `complete`.
현재 단계: V0~V5 `complete` (실기기 pause/resume 재수용 및 사용자 최종 승인 완료).
선행 조건: V0~V3 `complete`, S05 `sd-storage` `complete`.

## 1. 결정 요약

202609 프로젝트에서 현재 reader가 변환 없이 재생할 수 있는 WAV 다섯 개를 선별해
`sample-suite` SD asset으로 등록한다. `wav-sounds` page는 고정 등록 목록과 한 개의
현재 재생 panel을 2열로 보여 주며 다음 기능을 제공한다.

- 목록 행 또는 행의 재생 button을 누르면 해당 WAV를 즉시 재생한다.
- 새 WAV 재생은 기존 출력을 겹치지 않고 원자적으로 교체한다.
- 현재 파일의 재생 상태, 경과 시간, 전체 시간, 남은 시간과 progress bar를 표시한다.
- 큰 `재생/일시정지/계속 재생/다시 재생` primary button과 `중지` button을 제공한다.
- 화면 안에 실제 지원 형식을 짧게 소개하고 unsupported 원인을 구분한다.
- S03 시스템 음량·mute와 같은 단일 audio output을 사용한다.

V5 범위 확장으로 pause/resume은 구현한다. seek 가능한 slider, playlist 자동 재생, 반복 재생,
임의 SD directory 탐색과 편집 기능은 만들지 않는다. progress bar는 상태 표시 전용이며 touch로
탐색할 수 있는 control처럼 보이지 않게 한다.

## 2. 현재 상태와 재사용 판단

- `samples/suite/assets/sounds/volume_preview.wav` 한 개가 이미 S03 preview asset으로
  등록되어 있다. S06 목록에는 넣지 않고 S03의 역할을 유지한다.
- `boards/jc1060p470c/audio.*`는 `play`, `stop`, `set_volume`, 상태·오류만 공개한다.
  `AudioStatus`에 위치와 전체 시간이 없어 progress UI를 아직 만들 수 없다.
- `boards/jc1060p470c/internal/wav_reader.*`는 RIFF/WAVE PCM header와 bounded chunk를
  검사하며 WAV 전체를 RAM에 올리지 않는다.
- `boards/jc1060p470c/internal/audio_service.*`는 단일 worker, 길이 1 command queue,
  SD exclusive lease, 512-byte input과 1,024-byte PCM slot buffer를 이미 사용한다.
- `platform/windows/settings/memory_level_settings.*`는 preview WAV 전용 SDL output을
  소유한다. S06 player와 별도 SDL device를 만들지 않고 단일 Windows playback service로
  합친 뒤 S03 preview도 여기에 위임해야 한다.
- `../dev_202609_hmi/src/hardware/audio.*`와 `internal/audio_service.*`에는
  `position_ms`, `duration_ms`, `probe`가 이미 있으므로 필요한 부분만 현재 facade와
  streaming worker에 맞게 선별 이식한다. 진단용 pause/fade/audio-lab UI는 가져오지 않는다.
- S05의 storage lease와 전역 MSC lock은 그대로 소비한다. S06 때문에 USB MSC controller나
  raw-sector driver 내부를 수정하지 않는다.

## 3. 등록 WAV 후보와 asset 계약

### 3.1 최종 후보

2026-11-10 실제 header와 SHA-256을 조사한 결과다. 아래 다섯 파일은 현재 reader가 바로
지원하는 48 kHz, mono, PCM 16-bit다. 202609의 파일명은 보존하되 화면에는 중립적인
한국어·영어 이름과 설명을 catalog text로 표시한다.

| 화면 역할 | 202609 source / 최종 파일명 | 시간 | 크기 | SHA-256 |
| --- | --- | ---: | ---: | --- |
| 시작 알림 | `../dev_202609_hmi/assets/sounds/cook_start.wav` | 2.21 s | 212,046 B | `1E883E7DC78CD60FD04530B107A3E5ED5B9873FBBAA36D9D9C38BA6476B194E6` |
| 완료 알림 | `../dev_202609_hmi/assets/sounds/cook_end.wav` | 2.47 s | 237,390 B | `0C2CFF072B11A22A5CC7FC38A0F7FF0A13452B7FA048BC58849B044CFEF15FCD` |
| 열림 알림 | `../dev_202609_hmi/assets/sounds/lid_open.wav` | 2.04 s | 195,918 B | `86EA1DAD0B280C70D1FB9582D5656A446936A14C4E8942BCC956CEE800B826D3` |
| 닫힘 알림 | `../dev_202609_hmi/assets/sounds/lid_close.wav` | 2.16 s | 207,438 B | `39DC1FD1C350D148A5FE35815A3AF2BED0442EB6B7954CEB7D6DD971D52F3543` |
| 강조 알림 | `../dev_202609_hmi/assets/sounds/cooktech_start.wav` | 3.55 s | 341,070 B | `65B222DEAD400E2303D2FA46B37D2582685A205C2967C60A053F84FFA7A6096E` |

대상 경로는 모두 `samples/suite/assets/sounds/<filename>`이며 HMI bundle에서는
`hmi/sample-suite/assets/sounds/<filename>`으로 배치한다. 사용자가 같은 workspace의
202609 파일 재사용을 2026-11-10 명시적으로 요청한 사실을 provenance에 기록하고,
catalog에는 source path, 용도, expected hash, format과 재배포 확인 필드를 둔다.

### 3.2 이번 등록에서 제외할 파일

| 파일 | 조사 결과 | 처리 |
| --- | --- | --- |
| `1step.wav`, `2step.wav` | 24 kHz mono PCM 16-bit | 현재 sample-rate allowlist 밖이므로 제외 |
| `cook_step_start.wav`, `cook_step_stop.wav` | 24 kHz mono PCM 16-bit | 현재 sample-rate allowlist 밖이므로 제외 |
| `volume_preview.wav` | 44.1 kHz stereo PCM 16-bit | 지원되지만 S03 preview 전용으로 유지 |

24 kHz 파일을 몰래 resample하거나 reader 범위를 넓히지 않는다. 나중에 필요하면 source
보존, 변환 command와 새 hash를 별도 변경으로 기록하고 실제 codec clock을 검증한다.

### 3.3 등록 방식

- `samples/suite/pages/wav_sounds/wav_catalog.json`을 page-owned source of truth로 둔다.
- 각 entry는 `id`, asset relative path, title/description key, expected size/hash,
  channels, sample rate, bit depth, duration, provenance를 가진다.
- 최대 등록 수는 `kMaxRegisteredSounds = 8`로 제한한다. 현재는 다섯 개만 생성한다.
- 명시적 config를 받는 `tools/assets/audio/validate_wav_catalog.py`가 path traversal,
  duplicate ID/path, header, 지원 형식, size/hash와 locale key를 검사하고 작은 constexpr
  catalog header를 생성한다. 도구에 `sample-suite` 경로를 하드코딩하지 않는다.
- profile asset bundle의 `asset_manifest.json` hash 검증도 그대로 통과해야 한다.
- runtime에는 filename scan이나 JSON parsing을 두지 않는다. page는 generated bounded
  catalog view만 소비한다.

## 4. 지원 WAV 형식 안내 계약

현재 HMI reader가 실제로 재생하는 범위만 UI와 README에 표시한다.

- container/encoding: RIFF/WAVE, uncompressed integer PCM(format code 1)
- channel: mono 또는 stereo
- bit depth: 8, 16, 24-bit
- sample rate: 8, 16, 22.05, 32, 44.1, 48 kHz
- data: frame alignment과 byte rate가 header와 일치하는 한 개의 비어 있지 않은 data chunk
- stereo 입력은 현재 board의 mono speaker 출력에 맞게 downmix한 뒤 두 I2S slot에 복제

UI의 기본 안내는 `PCM WAV · mono/stereo · 8/16/24-bit · 8~48 kHz 지원` 한 줄과
sample-rate 목록 한 줄까지만 보여 준다. 압축 WAV(ADPCM, MP3-in-WAV), IEEE float,
24 kHz와 손상된 header는 지원하지 않는다는 설명을 작은 `지원 형식` card에 둔다.
두 locale에서 동적으로 wrap될 수 있으므로 paragraph width/height, 5 px 이상의 line spacing과
좌측 정렬을 명시하고 title과 body를 별도 label로 만든다.

## 5. 목표 UI/UX

### 5.1 1024×600 정보 구조

- 기존 page header, Back, 한국어/영어, light/dark control을 그대로 재사용한다.
- header 아래 24 px outer margin과 20 px column gap을 두고, 왼쪽 약 390 px는 등록 목록,
  오른쪽 나머지는 현재 재생과 지원 형식 영역으로 사용한다.
- 왼쪽 목록은 다섯 개의 64~68 px row를 표시한다. 각 row에는 이름, 짧은 용도/시간,
  48 px 이상의 원형 재생 button이 있다.
- row 전체와 재생 button은 같은 `재생` action을 수행한다. 중복 event가 발생하지 않게
  child event bubbling을 명시적으로 제어한다.
- 선택/재생 중 row는 theme accent border, 배경 tint와 `재생 중` badge로 한 번만 강조한다.
  색만으로 상태를 전달하지 않는다.
- 오른쪽 상단의 now-playing card에는 선택 음원 이름, 상태, format metadata,
  14 px 비상호작용 progress bar와 시간 정보를 둔다.
- 시간 행은 `경과 00:00`, `전체 02:21`, `남음 02:21` 세 값을 고정 폭으로 배치한다.
  duration을 아직 모르면 `--:--`를 표시하고 0으로 오인시키지 않는다.
- control row에는 primary `재생`/`다시 재생`과 secondary `중지`를 둔다. 재생 중이 아니면
  `중지`는 disabled style과 실제 disabled state를 함께 적용한다.
- 오른쪽 하단에 compact `지원 형식` card를 둔다. 세부 기술 문구가 player보다 시각적으로
  강해지지 않도록 body/status typography와 낮은 대비 surface를 사용한다.

### 5.2 상태와 interaction

- 최초 진입: 목록은 활성, player에는 `목록에서 음원을 선택하세요`와 0 progress를 표시한다.
- loading: 선택 row와 player에 spinner/`준비 중`을 표시하고 중복 요청은 마지막 요청만 유지한다.
- playing: 100 ms UI timer가 immutable snapshot만 읽어 progress와 세 시간을 갱신한다.
- completed: progress를 100%로 고정하고 `재생 완료`, primary action을 `다시 재생`으로 바꾼다.
- stopped: 현재 선택은 유지하고 위치는 0으로 되돌려 같은 파일을 다시 재생하기 쉽게 한다.
- replace: 다른 row를 누르면 이전 output/stream을 종료하고 새 파일 하나만 재생한다.
- missing/unsupported/error: 해당 row 또는 player에 정확한 오류와 재시도 action을 표시하며
  다른 등록 음원은 계속 사용할 수 있다.
- page 이탈, locale/theme rebuild와 MSC 전환 시작 시 즉시 stop을 요청하고 UI timer를 먼저
  제거한다. worker는 Board lifetime 동안 존재해도 file handle, output과 lease는 남지 않아야 한다.

animation은 progress indicator와 짧은 상태 전환에만 사용한다. 자동 scroll, waveform 장식,
과한 gradient나 재생과 무관한 motion은 넣지 않는다. light/dark 모두에서 accent와 disabled,
error 상태가 명확하고 모든 touch target은 최소 48 px로 유지한다.

## 6. 목표 아키텍처와 책임 경계

### 6.1 공통 playback 계약

`modules/audio/`에 domain-neutral `AudioPlaybackPort`, `PlaybackSnapshot`과 작은 controller를
둔다. contract는 다음만 노출한다.

- `play(path)`, `stop()`, `probe(path)`와 read-only `snapshot()`
- state: unavailable/idle/starting/playing/completed/stopped/error
- error: unavailable/path/file/unsupported/truncated/output/read/busy
- `position_ms`, `duration_ms`, monotonically increasing `revision`
- header metadata: channel, sample rate, bit depth, duration, file bytes

progress permille과 remaining time은 공통 controller가 bounded arithmetic으로 계산한다.
UI는 Board type, SDL handle, FILE, FreeRTOS queue와 SD handle을 알지 않는다. page-specific
catalog 이름/설명과 선택 정책은 `samples/suite/pages/wav_sounds/`가 소유한다.

### 6.2 HMI와 Board

- `boards/jc1060p470c/audio.*`의 public facade에 domain-neutral position/duration/probe와
  revision을 추가한다. internal object나 driver handle은 노출하지 않는다.
- `internal/wav_reader.*`에 byte count→milliseconds 계산을 현재 overflow 규칙에 맞춰
  선별 이식한다.
- worker는 chunk를 성공적으로 출력한 뒤 position을 증가시키며 EOF에서 duration으로 clamp한다.
- stop/replace/error에서 state, position, duration과 revision 전환 순서를 test로 고정한다.
- 512-byte input, 1,024-byte PCM slot과 8,192-byte worker stack을 기본 상한으로 유지한다.
  전체 WAV, 전체 catalog와 UI snapshot을 task stack에 올리지 않는다.
- HMI adapter는 public `Board::audio()`만 사용하고 `hardware/internal`을 include하지 않는다.

### 6.3 Windows와 단일 output owner

- `platform/windows/audio/`에 sample asset root를 받는 SDL playback adapter 한 개를 둔다.
- 최대 파일 크기와 catalog 등록 path만 허용하고 한 번에 파일 하나만 queue한다.
- SDL queued bytes와 source format으로 같은 position/duration snapshot을 만든다.
- S03 `MemoryLevelSettings` preview도 이 adapter에 위임해 서로 다른 SDL device나 중첩 output을
  만들지 않는다.
- HMI `PreferencesLevelSettings`도 기존 Board audio owner를 계속 공유한다.

### 6.4 profile 조립

- `profiles/runtime/app_contract.hpp`에는 platform-neutral playback port만 추가한다.
- `hosts/windows/main.cpp`와 `hosts/hmi/main.cpp`가 adapter 수명과 주입을 담당한다.
- `SampleSuiteApp`은 port view를 보관해 `PageContext`로 전달할 뿐 page ID별 분기를 만들지 않는다.
- `samples/suite/page_registry.json` descriptor가 `wav-sounds` source와 capability를 등록한다.
- Dashboard는 생성된 descriptor만 소비하며 WAV page header나 concrete type을 include하지 않는다.

## 7. 예상 변경 파일

| 책임 | 예상 경로 |
| --- | --- |
| WAV 원본 asset | `samples/suite/assets/sounds/*.wav` |
| 등록 source/생성물 | `samples/suite/pages/wav_sounds/wav_catalog.json`, `samples/suite/generated/wav_catalog.hpp` |
| catalog 검증 도구 | `tools/assets/audio/validate_wav_catalog.py`, `tools/tests/` |
| 공통 playback 계약/model | `modules/audio/include/hmi/audio/`, `modules/audio/src/`, `modules/audio/tests/` |
| HMI facade/stream | `boards/jc1060p470c/audio.*`, `boards/jc1060p470c/internal/audio_service.*`, `boards/jc1060p470c/internal/wav_reader.*` |
| platform adapter | `platform/windows/audio/`, `platform/hmi/audio/` |
| 단일 preview owner 연결 | `platform/windows/settings/memory_level_settings.*`, `platform/hmi/settings/preferences_level_settings.*` |
| profile host 조립 | `profiles/runtime/app_contract.hpp`, `hosts/windows/main.cpp`, `hosts/hmi/main.cpp` |
| page/model/tests | `samples/suite/pages/wav_sounds/` |
| registry/i18n/build | `samples/suite/page_registry.json`, `samples/suite/profile.json`, `samples/suite/i18n/`, `samples/suite/CMakeLists.txt` |
| 문서 | `README.md` |

generated file은 직접 손으로 수정하지 않고 source catalog와 generator를 통해 갱신한다.
C/C++ 파일을 실제 구현할 때 public API, ownership, lifetime, non-obvious streaming arithmetic에는
저장소 규칙에 맞는 유용한 한국어 comment를 추가한다.

## 8. 단계별 개발 계획

### V0. WAV 선정·등록 계약 — `complete`

**목표**

다섯 source WAV의 provenance와 format을 고정하고 재현 가능한 asset catalog를 만든다.

**범위**

- 3절의 다섯 WAV 복사와 source metadata
- bounded page catalog schema와 validator/generator
- 지원·제외 형식의 실제 header 검증
- playback/UI 구현은 제외

**산출물**

- 다섯 WAV asset과 expected hash
- `wav_catalog.json`, generated constexpr catalog/manifest
- validator test와 README의 지원 형식 초안

**자동 검증**

- validator `--check`가 다섯 entry의 header, duration, size/hash와 path를 확인한다.
- 24 kHz 네 파일은 unsupported fixture로 분류되고 bundle에 들어가지 않는다.
- `python tools/project.py bundle-assets --profile sample-suite` 결과에 다섯 WAV가 정확히 한 번
  포함되고 source와 bundle hash가 같다.

**수동 검증**

- 사용자가 다섯 음원의 화면 역할과 202609 source 재사용 metadata를 확인한다.

**종료 조건**

- source/target/hash/format/provenance가 고정되고 generator stale check가 통과한다.
- V1이 file name이나 runtime directory scan 없이 catalog view를 소비할 수 있다.

**의존**

- S05 complete.

**완료 근거**

- 202609 source의 48 kHz mono PCM 16-bit 다섯 파일을 asset에 복사하고 source/catalog/asset/
  bundle SHA-256 일치를 확인했다. bundle은 182 files로 생성됐다.
- validator 생성·stale check와 전용 Python test 5개가 통과했다. duplicate ID, path traversal,
  hash drift, truncated data와 24 kHz unsupported를 각각 검증한다.
- 실제 `1step.wav`, `2step.wav`, `cook_step_start.wav`, `cook_step_stop.wav` 네 파일이 모두
  `unsupported sample rate 24000`으로 제외됨을 확인했다.
- 2026-11-10 사용자의 202609 WAV 재사용 요청을 각 entry의 provenance 승인으로 기록했다.

### V1. Playback contract와 platform adapter — `complete`

**목표**

Windows와 HMI가 같은 상태·시간 의미로 한 WAV를 재생·중지·교체하고 progress를 보고한다.

**범위**

- `modules/audio` port/controller
- Board public audio facade와 streaming worker의 position/duration/probe
- Windows/HMI adapter와 S03 preview의 단일 output owner 연결
- page widget은 제외

**산출물**

- bounded playback snapshot과 request revision
- mock, SDL, Board adapter
- play/stop/replace/EOF/error/probe/overflow test

**자동 검증**

- position은 단조 증가하고 duration을 넘지 않으며 remaining은 underflow하지 않는다.
- stop은 위치를 0으로 만들고 replace는 이전 stream/lease를 닫은 뒤 새 revision만 노출한다.
- EOF는 completed를 한 번만 만들고 다시 재생할 수 있다.
- unsupported, truncated, missing, storage unavailable과 output error가 구분된다.
- S03 preview와 S06 playback이 한 output owner를 공유해 중첩 출력이 없다.
- `sizeof` guard, buffer/stack 계산과 Board boundary `rg`가 통과한다.

**수동 검증**

- V1에서는 SDL queued output과 Board streaming 경계를 자동 검증한다.
- 실제 청음, 즉시 중지와 빠른 교체의 감각 평가는 완성된 UI로 판단해야 하므로 V4 실기기
  수용으로 이관한다.

**종료 조건**

- mock과 두 platform adapter가 동일한 상태 전이를 만족한다.
- 전체 WAV eager load가 HMI 경로에 없고 SD lease가 stream 전체 수명 동안 유지된다.

**의존**

- V0 complete.

**완료 근거 (2026-11-10)**

- `modules/audio`의 non-owning playback port/controller와 Windows SDL, HMI Board adapter를
  추가하고 S03 preview와 S06이 host당 한 playback owner를 공유하도록 조립했다.
- 공통 progress 계산은 duration clamp, remaining underflow 방지와 32-bit 최대 duration의
  overflow 안전성을 직접 검사한다. Board WAV reader는 48 kHz duration과 24 kHz 거부를
  직접 검사한다.
- `sample-suite` Windows 35개 test, `main` Windows 2개 test와 도구 Python 54개 test가
  통과했고 4개 generated profile isolation이 `ready`다.
- HMI `sample-suite` build는 RAM 86,180/512,000 bytes(16.8%), Flash
  1,482,486/6,553,600 bytes(22.6%)로 통과했다. 공통 Board regression인 HMI `main` build도
  RAM 28,256 bytes(5.5%), Flash 661,610 bytes(10.1%)로 통과했다.
- HMI audio worker는 WAV 전체를 적재하지 않고 기존 bounded chunk streaming과 file lease를
  유지한다. UI/module/platform adapter 대상 Board internal 경로 검색 결과는 0건이다.
- audible quality와 실제 touch 기반 stop/replace 평가는 V4 Manual Verification Checklist에서
  사용자 근거로 닫는다.

### V2. 목록과 현재 재생 UI — `complete`

**목표**

5절의 2열 UI에서 한 번의 명확한 touch로 재생하고 모든 시간과 상태를 읽을 수 있다.

**범위**

- `wav-sounds` page model/widget와 100 ms snapshot poll
- 다섯 목록 row, now-playing card, progress/time, play/replay/stop
- 지원 형식 card, 한국어/영어, light/dark
- pause/seek/playlist는 제외

**산출물**

- page/controller와 state/interaction/navigation/destruction test
- catalog text·typography 등록
- four-state Windows captures: initial, playing, completed/error, dark English

**자동 검증**

- row와 child button은 재생 요청을 정확히 한 번 만든다.
- progress 0/중간/100%, elapsed/total/remaining format과 unknown duration을 검사한다.
- stop disabled state, replay, rapid row switch, missing/unsupported recovery를 검사한다.
- locale/theme rebuild와 page destroy 뒤 timer/widget callback이 남지 않는다.
- fixed text locale/placeholder/typography parity와 multiline bounds가 통과한다.

**수동 검증**

- 1024×600에서 목록과 player hierarchy, 48 px touch target, 세 시간 값과 progress가 한눈에
  구분되는지 두 theme와 두 locale에서 확인한다.
- 지원 형식 문구가 잘리거나 player보다 과도하게 강조되지 않는지 확인한다.

**종료 조건**

- 겹침·잘림·이중 event 없이 목록→재생→중지→다른 음원 흐름이 완료된다.
- 색상 없이도 선택/재생/error/disabled 상태를 구분할 수 있다.

**의존**

- V1 complete.

**완료 근거 (2026-11-10)**

- 등록 5행, 현재 재생, 읽기 전용 progress, 경과/전체/남은 시간, 재생/다시 재생/중지와
  지원 형식 안내를 1024×600 2열 UI로 구현했다. 주요 row와 button은 모두 48 px 이상이다.
- row 한 번 touch의 단일 request, 중간/완료 progress, replay/stop, unsupported 뒤 정상 항목
  recovery, 빠른 항목 교체와 destroy 뒤 timer 무동작을 direct state/interaction test로 확인했다.
- 한국어/영어 224개 catalog key·placeholder·typography parity와 fixed font render test가
  통과한다. 동적 시간 값의 `0-9`, `:`, `-` glyph도 control font에 명시적으로 포함했다.
- initial, playing, completed, dark-English 네 캡처를 `_artifacts/wav-sounds/`에서 직접
  확인했다. 목록/player hierarchy, 선택 border+상태 text, 세 시간 값과 format card에
  겹침·잘림이 없다.
- `sample-suite` Windows 37개 전체 test와 도구 Python 54개 test가 통과한다.
- 최신 font converter의 여러 줄 options 주석에서 임시 경로가 남아 `--check`가 흔들리던
  문제를 제거해 generate 직후 deterministic stale check가 통과한다.

### V3. Profile·storage·asset 통합 — `complete`

**목표**

page를 registry로 조립하고 asset 누락, MSC 소유권 전환과 page lifetime을 안전하게 처리한다.

**범위**

- page registry/profile/build input/i18n/generated asset 연결
- S05 global lock과 audio lease 통합
- asset 누락/손상/교체와 재진입
- 새 USB/MSC 정책 구현은 제외

**산출물**

- Dashboard `wav-sounds` descriptor
- Windows fixture와 HMI asset path adapter
- storage transition/lifetime integration test와 README 사용법

**자동 검증**

- MSC 진입 시작은 재생을 stop하고 stream lease 해제 뒤에만 raw owner를 허용한다.
- MSC owner 동안 play는 stable busy/unavailable로 거부되고 remount 후 재생된다.
- 누락/손상된 한 entry가 다른 등록 음원을 막지 않는다.
- page 왕복, locale/theme rebuild와 app destroy 뒤 stream/output/timer/callback/lease가 남지 않는다.
- Dashboard/host에 page ID별 include나 분기가 없다.
- asset bundle의 file count/size/hash와 profile isolation이 통과한다.

**수동 검증**

- Windows에서 Dashboard 진입·복귀, locale/theme 전환, missing asset 화면과 MSC fixture 흐름을
  확인한다.

**종료 조건**

- Windows sample-suite 전체 흐름과 asset bundle이 재현 가능하다.
- S05 controller, USB driver와 completed page 내부를 WAV 정책 때문에 수정하지 않는다.

**의존**

- V2 complete.

**완료 근거 (2026-11-10)**

- `wav-sounds`를 order 60의 generated page descriptor로 등록했다. host, Dashboard와
  `SampleSuiteApp`에는 page ID/type include나 분기가 없음을 검색 결과 0건으로 확인했다.
- 실제 SDL adapter가 catalog의 다섯 WAV를 모두 probe해 file bytes, 48 kHz mono,
  16-bit와 duration 일치를 확인하고 queued play/stop과 상대 경로 거부를 통과했다.
- 앱 통합 test는 Dashboard 진입, missing file 오류 뒤 정상 행 recovery, 한국어→영어,
  light→dark 재구성, 빠른 행 교체, page 이탈 정리를 검증한다.
- 재생 중 MSC 잠금 시작에서 공유 playback owner를 정확히 한 번 stop하고, 반복 refresh는
  중복 stop하지 않으며, PC owner 동안 navigation을 거부하고 remount 뒤 같은 page에서
  다시 재생됨을 확인했다.
- asset bundle은 182 files로 재생성했고 WAV source/catalog/bundle hash와 HMI asset root
  `hmi/sample-suite/assets/sounds/`가 일치한다. 4개 profile isolation도 `ready`다.
- `README.md`에 다섯 등록 이름, 조작법, 지원/비지원 PCM 형식, SD 경로, MSC 제한과
  실기기 수동 확인 순서를 기록했다.
- Windows `sample-suite` 전체 39개 test, Python 54개 test, typography/WAV stale check와
  `git diff --check`가 통과했다.

### V4. 최종 build·메모리 — `complete`

**목표**

변경 범위의 자동 gate와 실제 HMI 청취·touch·storage 수용을 완료한다.

**범위**

- Windows/HMI affected profile build와 direct tests
- Board 공통 변경 때문에 `main` HMI regression build
- RAM/flash/object/buffer/stack 기록
- 사용자의 직접 upload 뒤 실기기 수용

**산출물**

- 최종 command 결과와 memory 수치
- light/dark, ko/en Windows capture
- 다섯 음원과 진행률/시간의 실기기 확인 기록

**자동 검증**

- 아래 Automated Verification Checklist 전체를 실행한다.

**수동 검증**

- 아래 Manual Verification Checklist 전체를 사용자가 확인한다.
- Windows와 HMI에서 다섯 WAV의 실제 재생, 즉시 중지와 빠른 교체가 자연스럽게 들리는지
  완성된 player UI로 확인한다.

**자동 검증 완료 근거 (2026-11-10)**

- HMI `sample-suite` 최종 build는 RAM 86,180/512,000 bytes(16.8%), Flash
  1,501,678/6,553,600 bytes(22.9%)로 통과했다. V1 기준 대비 RAM 증가는 0 bytes이고,
  page·font·audio code를 포함한 Flash 증가는 19,192 bytes다.
- Board 공통 변경 회귀용 HMI `main` build는 RAM 28,256/512,000 bytes(5.5%), Flash
  661,610/6,553,600 bytes(10.1%)로 통과했으며 기존 기록과 동일하다.
- HMI ELF debug type 기준 객체 크기는 `WavCatalogEntry` 40 bytes(5개 총 200 bytes),
  `AudioPlayback` 24 bytes, `BoardWavPlayback` 328 bytes, `WavSoundsPage` 208 bytes,
  `AudioService` 32 bytes다. compile-time guard는 catalog 최대 8개, controller 64 bytes,
  page 416 bytes, service 80 bytes로 유지한다.
- worker는 WAV 전체를 적재하지 않는다. 단일 queue payload는 258 bytes이고, streaming
  automatic buffer는 input 512 bytes + stereo output 1,024 bytes로 고정되어 있다.
  최초 V4 build의 worker stack은 6,144 bytes였으나 V5 실기기 재부팅 제보 뒤 202609의
  검증된 8,192-byte 예산으로 복구했다.
- Windows `sample-suite` 39개 test와 `main` check, Python 54개 test, 4개 profile
  isolation, 182-file asset bundle, typography/WAV stale check, boundary search와
  `git diff --check`가 통과했다.
- Windows light/dark 및 ko/en capture는 `_artifacts/wav-sounds/`에 보관했다.

**종료 조건**

- 모든 자동 check와 build·memory 근거가 통과한다.
- 실기기 수동 수용은 V5로 이동하며 S06 최종 완료 선언은 V5 통과 전까지 유보한다.

**의존**

- V3 complete, 사용자의 직접 firmware upload.

**완료 근거**

- Windows/HMI build, direct test, isolation, asset/font/WAV stale check와 memory 근거가
  모두 기록됐다.
- 2026-11-10 사용자가 실기기 수동 수용을 유보하고 S07을 먼저 진행하도록
  명시적으로 요청했다.

### V5. 실기기 결함 수정·pause/resume·재수용 — `complete`

**목표**

V4에서 분리한 WAV 청취·touch·storage·lifetime 수동 검증을 실기기에서 완료한다.

**범위·산출물**

- 문서 끝의 Manual Verification Checklist 전체
- `play → pause → resume`에서 경과 위치와 storage lease를 유지하는 공통 playback 계약
- 재생 중 primary button은 `일시정지`, paused 상태에서는 `계속 재생`, stop/complete에서는
  처음부터 `재생/다시 재생`으로 동작하는 UI
- 다른 목록 행 선택은 현재 session을 정리하고 새 파일을 처음부터 재생하며, `중지`는 위치를
  0으로 되돌린다.
- seek/slider, playlist, repeat는 이번 범위에 포함하지 않는다.
- 사용자의 S06 최종 완료 승인

**자동 검증**

- V4 이후 WAV/audio/storage 관련 코드가 바뀌었다면 해당 direct test와 HMI build를
  재실행한다.
- 공통 controller와 Windows/HMI adapter에서 pause/resume 상태, 위치 유지, stop reset,
  replace와 page destruction을 검증한다.
- primary button 실제 좌표 click으로 play/pause/resume/replay 전환과 시간 고정을 검증한다.

**수동 검증·종료 조건**

- Manual Verification Checklist 전체와 명시적 사용자 승인이 있어야 S06을
  `complete`로 닫는다.
- 실제 HMI에서 pause 중 소리와 시간이 멈추고, resume 시 같은 위치부터 자연스럽게 이어져야
  한다.

**의존**

- V4 complete. S07 A0의 선행 조건에서는 2026-11-10 사용자 범위 변경으로 제외한다.

**2026-11-10 실기기 실패 및 수정 근거**

- 사용자가 등록 WAV 목록 touch 뒤 반응이 느리고 잠시 후 장치 앱이 재시작된다고 제보했다.
- `AudioService::play()`가 LVGL touch callback에서 SD `stat()`를 직접 호출하던 경로를
  제거했다. callback은 bounded queue 갱신과 `starting` 상태 공개만 수행하고, file open,
  header 검사와 오류 전환은 storage lease를 소유한 worker가 처리한다.
- 이식 과정에서 202609의 검증값 8,192 bytes보다 작아진 6,144-byte audio worker stack을
  8,192 bytes로 복구했다. 1.5 KiB PCM local buffer와 I2S/stdio/codec call frame이 겹칠 때의
  stack overflow 재부팅 위험을 줄인다.
- 수정 뒤 현재 workspace 기준 HMI `sample-suite` build가 RAM 87,552/512,000 bytes(17.1%),
  Flash 1,560,370/6,553,600 bytes(23.8%)로 통과했다. 이 수치는 동시에 추가된 S07 source도
  포함하므로 V4 대비 WAV 수정 단독 증분으로 해석하지 않는다.
- 직접 upload는 수행하지 않았다. 사용자의 재업로드 후 목록 touch 응답, 다섯 파일 재생,
  빠른 교체와 serial reset/stack 오류 부재를 다시 확인해야 V5를 닫는다.
- 사용자의 첫 재검증에서 다섯 목록 행 모두 실제 touch에 반응하지 않는 결함이 확인됐다.
  각 행의 투명한 text column이 LVGL 기본 `CLICKABLE` flag로 hit target을 가로채면서 row의
  `LV_EVENT_SHORT_CLICKED` callback이 호출되지 않은 것이 원인이었다.
- row 내부 text column과 label을 장식용 non-clickable 객체로 명시해 65 px 행 전체가 하나의
  touch target이 되게 했다. 기존처럼 row 객체에 event를 직접 주입하지 않고 title 실제
  화면 좌표를 SDL pointer로 누르는 회귀 test를 추가했다.
- 좌표 기반 WAV interaction test와 Windows `sample-suite` 전체 41개 test가 통과했다.
  최종 HMI build도 RAM 87,552/512,000 bytes(17.1%), Flash
  1,560,812/6,553,600 bytes(23.8%)로 통과했다.
- 사용자는 실기기 재생 확인 중 primary `재생` button이 항상 파일을 처음부터 다시 시작하는
  동작을 지적하고 음악에도 자연스러운 pause/resume을 요청했다. V5를 다시 `in_progress`로
  열고 public audio facade, 공통 port/controller, Windows/HMI adapter와 page UI를 같은 상태
  계약으로 확장한다.
- `playing → pausing → paused → resuming → playing` 상태를 공통 port, Board facade와 HMI
  worker에 추가했다. pause 동안 열린 `FILE*`, storage lease, byte offset과 position을 유지하고,
  resume은 같은 chunk 경계에서 이어지며 stop만 position을 0으로 초기화한다.
- primary button은 재생 중 `일시정지`, paused 상태에서 `계속 재생`, 완료 뒤 `다시 재생`으로
  바뀐다. 전환 중에는 primary를 잠시 disable하고 `중지`는 계속 허용한다.
- 공통 controller, SDL adapter, page state, 실제 좌표 interaction, app integration direct test
  5개와 Windows `sample-suite` 전체 45개 test가 통과했다. SDL pause 대기 동안 position이
  고정되고 resume 직후 같은 위치 이상에서 계속됨을 검증했다.
- typography catalog 262개 stale check, Python 54개 test와 4개 profile isolation이 통과했다.
  HMI `sample-suite` build는 RAM 87,616/512,000 bytes(17.1%), Flash
  1,562,726/6,553,600 bytes(23.8%)로 통과했다. `AudioPlayback`은 HMI에서 32 bytes로
  64-byte guard 안이며, `AudioService`의 bounded buffer와 8,192-byte worker stack은 유지된다.
- Board facade 회귀용 HMI `main` build도 RAM 28,256/512,000 bytes(5.5%), Flash
  661,610/6,553,600 bytes(10.1%)로 통과했다.
- 직접 upload는 수행하지 않았다. 자동 gate는 끝났지만 실제 보드에서 pause 시 소리·시간이
  멈추고 resume이 같은 위치에서 자연스럽게 이어지는지 사용자 확인이 필요해 V5를
  `blocked`로 둔다.
- 2026-11-10 사용자가 수정 firmware의 실기기 동작이 정상임을 확인하고 “잘 된다 최종
  완료해”라고 명시적으로 승인했다. 목록 touch, 재생·일시정지·같은 위치 이어 재생과
  안정성 재수용 gate를 통과한 것으로 기록해 V5와 S06을 `complete`로 닫는다.

## 9. 단계 게이트 요약

| 단계 | 상태 | 핵심 종료 조건 | 다음 단계 허용 |
| --- | --- | --- | --- |
| V0 | `complete` | 다섯 WAV·catalog·hash·provenance 고정 | V1 |
| V1 | `complete` | 공통 상태/시간과 Windows/HMI 단일 output 통과 | V2 |
| V2 | `complete` | 목록+player UI와 두 locale/theme 수용 | V3 |
| V3 | `complete` | registry/storage/asset/lifetime 통합 통과 | V4 |
| V4 | `complete` | build·memory 자동 gate | V5, S07 A0 |
| V5 | `complete` | pause/resume·touch·재부팅 수정의 실기기 재수용과 사용자 승인 | S06 complete |

V0~V5의 자동·수동 gate와 사용자의 최종 승인이 모두 완료되어 S06을 `complete`로 닫았다.

## 10. 위험과 완화

| 위험 | 영향 | 완화 |
| --- | --- | --- |
| 24 kHz 파일을 지원으로 오인 | 재생 거부 또는 clock 오류 | V0 header validator와 explicit allowlist, 대상에서 제외 |
| progress가 output보다 앞서감 | 잘못된 UX | 실제 chunk write 성공 뒤 position 증가, duration clamp |
| position/duration torn read | 시간 역행·100% 초과 | atomic snapshot/revision 계약과 bounded controller 계산 |
| 빠른 row touch의 late 상태 | 이전 파일이 현재로 표시 | monotonically increasing revision, 마지막 request만 표시 |
| S03 preview와 S06 device 충돌 | 겹침·SDL/HMI output 실패 | host당 단일 playback owner와 preview 위임 |
| WAV 전체를 embedded RAM/stack에 적재 | stack overflow·heap 고갈 | 기존 streaming chunk 유지, catalog max 8, size guard |
| page 이탈 뒤 worker/lease 잔존 | MSC 진입 실패·소리 지속 | timer 제거→stop→stream close/lease release 순서 test |
| missing/corrupt asset 하나가 page를 막음 | 전체 기능 사용 불가 | entry-local error와 다른 row 재생 허용 |
| 기술 안내가 UI를 압도하거나 wrap | player 가독성 저하 | compact card, 별도 title/body, width/height/5 px line spacing |
| row와 child button 이중 event | 두 번 restart | event bubbling 제어와 request count interaction test |
| generated catalog와 asset drift | 잘못된 이름/hash/path | source catalog generator stale check와 bundle hash 검증 |

## 11. 후속 변경 격리 확인

- 새 WAV 추가/삭제: `wav_catalog.json`, asset, i18n만 변경하고 playback module과 Board는
  수정하지 않는다.
- 새 board 추가: 새 platform adapter만 구현하고 page/model/catalog는 수정하지 않는다.
- 새 PCM sample rate 또는 encoding 지원: reader/output capability와 validator를 함께 변경하고
  UI support text는 실제 adapter capability에서 생성한다.
- seek를 나중에 추가: capability가 확인된 뒤 port/controller/UI를 별도 계획으로 확장한다.
  이번 단계에서 보이지 않는 seek contract를 미리 만들지 않는다.
- locale 추가: catalog text와 font generation만 변경하며 WAV binary와 hardware code는 건드리지 않는다.

이 변형들이 completed S03/S05 내부나 Dashboard의 page ID 분기를 반복해서 요구하면 조립 경계가
잘못된 것으로 보고 다음 단계 전에 바로잡는다.

## Automated Verification Checklist

- [x] `python tools/assets/audio/validate_wav_catalog.py --config samples/suite/pages/wav_sounds/wav_catalog.json --check`가 통과한다.
- [x] 다섯 WAV의 header, duration, size와 SHA-256이 source/catalog/bundle에서 일치한다.
- [x] catalog maximum/duplicate/path traversal/unsupported/truncated fixture test가 통과한다.
- [x] play/stop/replace/replay/EOF/error와 position/duration/remaining/revision test가 통과한다.
- [x] play/pause/resume에서 position을 유지하고 stop에서 0으로 reset하는 test가 통과한다.
- [x] primary button 실제 좌표 click으로 pause/resume label·상태·시간 고정을 검증한다.
- [x] HMI 경로에 전체 WAV eager load나 큰 automatic buffer가 없다.
- [x] S03 preview와 S06 player가 host당 단일 output owner를 사용한다.
- [x] MSC owner 동안 재생이 거부되고 remount 후 재생되며 owner≤1이 유지된다.
- [x] row/button 단일 request, progress/time, locale/theme, navigation/destruction test가 통과한다.
- [x] page 종료 후 timer, stream, output, file, callback과 storage lease가 남지 않는다.
- [x] `python tools/project.py bundle-assets --profile sample-suite`가 통과한다.
- [x] `python tools/project.py check --host windows --profile sample-suite`가 통과한다.
- [x] `python tools/project.py check --host windows --profile main`이 통과한다.
- [x] `python -m unittest discover -s tools/tests -v`가 통과한다.
- [x] `python tools/project.py check --scope isolation`이 통과한다.
- [x] `python tools/project.py build --host hmi --profile sample-suite`가 통과한다.
- [x] Board 공통 audio 변경 후 `python tools/project.py build --host hmi --profile main`이 통과한다.
- [x] UI/module/platform adapter에 Board internal, FreeRTOS, SDL 또는 page-specific 역의존이 없다.
- [x] catalog/controller/page/facade 객체, PCM buffer, worker stack과 RAM/flash 변화가 기록된다.
- [x] `README.md`에 등록 음원, 조작법, 지원/비지원 WAV 형식, SD asset 경로와 MSC 제한이 기록된다.
- [x] `git diff --check`가 통과한다.

## Manual Verification Checklist

- [x] 사용자가 다섯 등록 이름과 실제 소리가 의도한 역할과 일치함을 확인한다.
- [x] 목록 행을 한 번 누르면 즉시 해당 음원이 재생되고 활성 row가 명확히 강조된다.
- [x] progress가 단조 증가하며 경과·전체·남은 시간이 실제 재생과 자연스럽게 맞는다.
- [x] 재생 완료 시 100%, `재생 완료`, `다시 재생` 상태가 일치한다.
- [x] `중지`가 즉시 소리를 끄고 위치를 0으로 돌리며 idle에서는 disabled다.
- [x] `일시정지`가 소리와 진행 시간을 멈추고 `계속 재생`이 같은 위치에서 자연스럽게 이어진다.
- [x] 여러 row를 빠르게 눌러도 소리가 겹치거나 이전 음원의 상태가 남지 않는다.
- [x] S03 음량 0~10과 mute가 다섯 음원과 preview에 일관되게 적용된다.
- [x] 누락·손상·unsupported 항목 오류 뒤 다른 정상 음원을 재생할 수 있다.
- [x] page 이탈, locale/theme 전환과 재진입 뒤 소리·timer·callback이 남지 않는다.
- [x] MSC 전환 중 재생이 차단되고 HMI 복귀 후 정상 재생된다.
- [x] 한국어/영어와 light/dark에서 목록, 세 시간 값, button, format 안내가 잘리지 않는다.
- [x] 모든 주요 touch target이 실제 LCD에서 안정적으로 눌리고 progress bar가 seek control로 오인되지 않는다.
- [x] serial log에 watchdog, stack overflow, allocation 실패, 동시 storage owner와 audio read 오류가 없다.
- [x] 2026-11-10 사용자가 S06 완료를 명시적으로 승인했다.

# QR renderer provenance

S07은 저장소가 이미 사용하는 LVGL 8.4의 `lv_qrcode` widget을 호출한다. 별도의 QR 구현이나 외부 binary asset은 복사하지 않는다.

- Widget: LVGL `src/extra/libs/qrcode/lv_qrcode.c`
- Encoder: LVGL에 포함된 Nayuki `qrcodegen.c`
- License: 저장소의 LVGL dependency가 따르는 MIT license
- S07 소유 코드: `wifi_ap_web_qr.cpp`는 Wi-Fi와 제어 URL payload만 고정 크기 버퍼에 작성한다.

따라서 upstream QR encoder의 수정본이나 별도 third-party source는 S07 page 디렉터리에 존재하지 않는다.

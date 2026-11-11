/*
 * SPDX-FileCopyrightText: 2026 HMI starter contributors
 * SPDX-License-Identifier: MIT
 */

#include <cstddef>
#include <cstdint>
#include <new>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#ifndef FACE_PROBE_ENABLED
#define FACE_PROBE_ENABLED 1
#endif

#if FACE_PROBE_ENABLED
#include "human_face_detect.hpp"

extern const uint8_t human_face_jpg_start[] asm("_binary_human_face_jpg_start");
extern const uint8_t human_face_jpg_end[] asm("_binary_human_face_jpg_end");
#endif

namespace {

const char *kTag = "face_probe";

struct HeapSnapshot {
    size_t internal_free;
    size_t internal_largest;
    size_t spiram_free;
    size_t spiram_largest;
    UBaseType_t stack_high_water_bytes;
};

HeapSnapshot capture_heap_snapshot()
{
    return {
        .internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        .internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
        .spiram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        .spiram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
        // ESP-IDF의 StackType_t는 uint8_t이므로 high-water 값은 byte 단위다.
        .stack_high_water_bytes = uxTaskGetStackHighWaterMark(nullptr),
    };
}

void log_heap_snapshot(const char *phase, const HeapSnapshot &snapshot)
{
    ESP_LOGI(kTag,
             "memory phase=%s internal_free=%zu internal_largest=%zu "
             "spiram_free=%zu spiram_largest=%zu stack_high_water_bytes=%u",
             phase,
             snapshot.internal_free,
             snapshot.internal_largest,
             snapshot.spiram_free,
             snapshot.spiram_largest,
             static_cast<unsigned>(snapshot.stack_high_water_bytes));
}

#if FACE_PROBE_ENABLED

constexpr int kFixtureWidth = 320;
constexpr int kFixtureHeight = 240;
constexpr size_t kExpectedKeypointValues = 10;

bool validate_fixture_result(const dl::detect::result_t &result)
{
    if (result.box.size() < 4 || result.keypoint.size() < kExpectedKeypointValues) {
        ESP_LOGE(kTag, "invalid result shape box=%zu keypoint=%zu",
                 result.box.size(), result.keypoint.size());
        return false;
    }

    const auto &point = result.keypoint;
    const bool box_in_bounds = result.box[0] >= 0 && result.box[1] >= 0 &&
                               result.box[2] < kFixtureWidth && result.box[3] < kFixtureHeight &&
                               result.box[0] < result.box[2] && result.box[1] < result.box[3];

    // ESP-DL 3.1.0의 순서는 왼눈, 왼쪽 입꼬리, 코, 오른눈, 오른쪽 입꼬리다.
    const bool landmark_geometry =
        point[0] < point[6] &&      // 왼눈은 오른눈보다 왼쪽이다.
        point[2] < point[8] &&      // 왼쪽 입꼬리는 오른쪽 입꼬리보다 왼쪽이다.
        point[1] < point[3] &&      // 왼눈은 왼쪽 입꼬리보다 위에 있다.
        point[7] < point[9] &&      // 오른눈은 오른쪽 입꼬리보다 위에 있다.
        point[4] > point[0] && point[4] < point[6] &&
        point[5] > point[1] && point[5] < point[3];

    ESP_LOGI(kTag,
             "score=%.6f box=[%d,%d,%d,%d] "
             "left_eye=[%d,%d] left_mouth=[%d,%d] nose=[%d,%d] "
             "right_eye=[%d,%d] right_mouth=[%d,%d]",
             result.score,
             result.box[0], result.box[1], result.box[2], result.box[3],
             point[0], point[1], point[2], point[3], point[4], point[5],
             point[6], point[7], point[8], point[9]);
    return result.score >= 0.5F && box_in_bounds && landmark_geometry;
}

bool run_fixture_probe()
{
    // JPEG decode 결과는 heap에 존재하며 detector 실행이 끝날 때까지 lifetime을 유지한다.
    const dl::image::jpeg_img_t fixture = {
        .data = const_cast<uint8_t *>(human_face_jpg_start),
        .width = kFixtureWidth,
        .height = kFixtureHeight,
        .data_size = static_cast<uint32_t>(human_face_jpg_end - human_face_jpg_start),
    };
    dl::image::img_t image = {};
    image.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;
    ESP_LOGI(kTag, "fixture jpeg_bytes=%u decoder_fast=%d",
             fixture.data_size, CONFIG_JD_FASTDECODE);
    const esp_err_t decode_status = sw_decode_jpeg(fixture, image, true);
    if (decode_status != ESP_OK || image.data == nullptr) {
        ESP_LOGE(kTag, "fixture JPEG decode failed status=%s",
                 esp_err_to_name(decode_status));
        return false;
    }

    log_heap_snapshot("decoded", capture_heap_snapshot());
    HumanFaceDetect *detector = new (std::nothrow) HumanFaceDetect();
    if (detector == nullptr) {
        ESP_LOGE(kTag, "detector allocation failed");
        heap_caps_free(image.data);
        return false;
    }

    log_heap_snapshot("model_loaded", capture_heap_snapshot());
    const int64_t started_us = esp_timer_get_time();
    const auto &results = detector->run(image);
    const int64_t elapsed_us = esp_timer_get_time() - started_us;
    log_heap_snapshot("inference_done", capture_heap_snapshot());

    bool valid = results.size() == 1;
    if (!valid) {
        ESP_LOGE(kTag, "expected one face result count=%zu", results.size());
    } else {
        valid = validate_fixture_result(results.front());
    }
    ESP_LOGI(kTag, "fixture inference_us=%lld result_count=%zu",
             static_cast<long long>(elapsed_us), results.size());

    delete detector;
    heap_caps_free(image.data);
    log_heap_snapshot("released", capture_heap_snapshot());
    return valid;
}

#endif

} // namespace

extern "C" void app_main(void)
{
    ESP_LOGI(kTag, "JC1060P470C-I-W-Y C0.5 face fixture probe start");
    // USB CDC monitor가 reset 뒤 다시 연결되기 전에 짧은 fixture 검사가 끝나지 않도록 기다린다.
    vTaskDelay(pdMS_TO_TICKS(10000));
    ESP_LOGI(kTag, "monitor grace period complete; fixture probe begins");
    log_heap_snapshot("startup", capture_heap_snapshot());
#if FACE_PROBE_ENABLED
    if (run_fixture_probe()) {
        ESP_LOGI(kTag, "C0_5_FACE_FIXTURE_PASS");
    } else {
        ESP_LOGE(kTag, "C0_5_FACE_FIXTURE_FAIL");
    }
#else
    ESP_LOGI(kTag, "C0_5_FACE_FEATURE_OFF_PASS");
#endif
}

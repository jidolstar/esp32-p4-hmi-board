/*
 * SPDX-FileCopyrightText: 2026 HMI starter contributors
 * SPDX-License-Identifier: MIT
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <inttypes.h>
#include <new>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "human_face_detect.hpp"
#include "linux/videodev2.h"
#include "soc/gpio_num.h"

namespace {

const char *kTag = "camera_face_probe";

constexpr int kCameraSccbPort = 0;
constexpr gpio_num_t kCameraSccbSda = GPIO_NUM_7;
constexpr gpio_num_t kCameraSccbScl = GPIO_NUM_8;
constexpr int kCameraSccbFrequencyHz = 100000;
constexpr uint32_t kFrameWidth = 1288;
constexpr uint32_t kFrameHeight = 728;
constexpr uint32_t kBufferCount = 2;
constexpr uint32_t kWarmupFrames = 5;
constexpr uint32_t kDetectionAttempts = 30;
constexpr int kFrameTimeoutSeconds = 2;
constexpr size_t kExpectedKeypointValues = 10;

struct MappedBuffer {
    void *address = nullptr;
    size_t length = 0;
};

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
void log_heap_snapshot(const char *phase)
{
    const HeapSnapshot snapshot = capture_heap_snapshot();
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

void fourcc_to_text(uint32_t fourcc, char text[5])
{
    text[0] = static_cast<char>(fourcc & 0xffU);
    text[1] = static_cast<char>((fourcc >> 8U) & 0xffU);
    text[2] = static_cast<char>((fourcc >> 16U) & 0xffU);
    text[3] = static_cast<char>((fourcc >> 24U) & 0xffU);
    text[4] = '\0';
}

esp_err_t initialize_camera()
{
    const esp_video_init_csi_config_t csi_config = {
        .sccb_config = {
            .init_sccb = true,
            .i2c_config = {
                .port = kCameraSccbPort,
                .scl_pin = kCameraSccbScl,
                .sda_pin = kCameraSccbSda,
            },
            .freq = kCameraSccbFrequencyHz,
        },
        .reset_pin = GPIO_NUM_NC,
        .pwdn_pin = GPIO_NUM_NC,
    };
    const esp_video_init_config_t video_config = {
        .csi = &csi_config,
    };

    ESP_LOGI(kTag, "init sensor=OV02C10 expected_sccb=0x36 sda=%d scl=%d freq=%d",
             kCameraSccbSda, kCameraSccbScl, kCameraSccbFrequencyHz);
    return esp_video_init(&video_config);
}

esp_err_t wait_for_frame(int fd)
{
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    struct timeval timeout = {
        .tv_sec = kFrameTimeoutSeconds,
        .tv_usec = 0,
    };
    const int result = select(fd + 1, &read_fds, nullptr, nullptr, &timeout);
    if (result == 0) {
        ESP_LOGE(kTag, "frame timeout seconds=%d", kFrameTimeoutSeconds);
        return ESP_ERR_TIMEOUT;
    }
    if (result < 0) {
        ESP_LOGE(kTag, "select failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

uint64_t box_area(const dl::detect::result_t &result)
{
    if (result.box.size() < 4) {
        return 0;
    }
    const int width = std::max(0, result.box[2] - result.box[0]);
    const int height = std::max(0, result.box[3] - result.box[1]);
    return static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
}

bool validate_and_log_result(const dl::detect::result_t &result)
{
    if (result.box.size() < 4 || result.keypoint.size() < kExpectedKeypointValues) {
        ESP_LOGE(kTag, "invalid result shape box=%zu keypoint=%zu",
                 result.box.size(), result.keypoint.size());
        return false;
    }

    const auto &point = result.keypoint;
    const bool box_in_bounds = result.box[0] >= 0 && result.box[1] >= 0 &&
                               result.box[2] < static_cast<int>(kFrameWidth) &&
                               result.box[3] < static_cast<int>(kFrameHeight) &&
                               result.box[0] < result.box[2] && result.box[1] < result.box[3];
    bool points_in_bounds = true;
    for (size_t index = 0; index < kExpectedKeypointValues; index += 2) {
        points_in_bounds = points_in_bounds && point[index] >= 0 && point[index + 1] >= 0 &&
                           point[index] < static_cast<int>(kFrameWidth) &&
                           point[index + 1] < static_cast<int>(kFrameHeight);
    }

    // raw 배열은 왼눈, 왼쪽 입꼬리, 코, 오른눈, 오른쪽 입꼬리 순서다.
    ESP_LOGI(kTag,
             "score=%.6f box=[%d,%d,%d,%d] "
             "left_eye=[%d,%d] left_mouth=[%d,%d] nose=[%d,%d] "
             "right_eye=[%d,%d] right_mouth=[%d,%d]",
             result.score,
             result.box[0], result.box[1], result.box[2], result.box[3],
             point[0], point[1], point[2], point[3], point[4], point[5],
             point[6], point[7], point[8], point[9]);
    return result.score >= 0.5F && box_in_bounds && points_in_bounds;
}

esp_err_t capture_and_detect()
{
    esp_err_t ret = ESP_FAIL;
    MappedBuffer buffers[kBufferCount];
    struct v4l2_format format = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .fmt = {
            .pix = {
                .width = kFrameWidth,
                .height = kFrameHeight,
                .pixelformat = V4L2_PIX_FMT_RGB565,
            },
        },
    };
    char fourcc[5] = {};
    struct v4l2_requestbuffers request = {
        .count = kBufferCount,
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
    };
    HumanFaceDetect *detector = nullptr;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bool detected = false;
    uint32_t frame_number = 0;

    int fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDONLY | O_NONBLOCK);
    ESP_RETURN_ON_FALSE(fd >= 0, ESP_FAIL, kTag, "open %s failed",
                        ESP_VIDEO_MIPI_CSI_DEVICE_NAME);

    ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_S_FMT, &format) == 0, ESP_FAIL,
                      cleanup, kTag, "VIDIOC_S_FMT RGB565 1288x728 failed");

    fourcc_to_text(format.fmt.pix.pixelformat, fourcc);
    ESP_LOGI(kTag, "format width=%" PRIu32 " height=%" PRIu32
                   " fourcc=%s sizeimage=%" PRIu32,
             format.fmt.pix.width, format.fmt.pix.height, fourcc, format.fmt.pix.sizeimage);
    ESP_GOTO_ON_FALSE(format.fmt.pix.width == kFrameWidth &&
                          format.fmt.pix.height == kFrameHeight &&
                          format.fmt.pix.pixelformat == V4L2_PIX_FMT_RGB565,
                      ESP_FAIL, cleanup, kTag, "driver changed requested RGB565 format");

    ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_REQBUFS, &request) == 0, ESP_FAIL,
                      cleanup, kTag, "VIDIOC_REQBUFS failed");
    ESP_GOTO_ON_FALSE(request.count == kBufferCount, ESP_FAIL, cleanup, kTag,
                      "driver returned buffers=%" PRIu32 " expected=%" PRIu32,
                      request.count, kBufferCount);

    for (uint32_t index = 0; index < kBufferCount; ++index) {
        struct v4l2_buffer buffer = {
            .index = index,
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
        };
        ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_QUERYBUF, &buffer) == 0, ESP_FAIL,
                          cleanup, kTag, "VIDIOC_QUERYBUF index=%" PRIu32 " failed", index);
        buffers[index].length = buffer.length;
        buffers[index].address = mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE,
                                      MAP_SHARED, fd, buffer.m.offset);
        ESP_GOTO_ON_FALSE(buffers[index].address != MAP_FAILED, ESP_FAIL,
                          cleanup, kTag, "mmap index=%" PRIu32 " failed", index);
        ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_QBUF, &buffer) == 0, ESP_FAIL,
                          cleanup, kTag, "initial QBUF index=%" PRIu32 " failed", index);
    }
    log_heap_snapshot("camera_buffers_ready");

    detector = new (std::nothrow) HumanFaceDetect();
    ESP_GOTO_ON_FALSE(detector != nullptr, ESP_ERR_NO_MEM, cleanup, kTag,
                      "detector allocation failed");
    log_heap_snapshot("model_loaded");

    ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_STREAMON, &type) == 0, ESP_FAIL,
                      cleanup_detector, kTag, "VIDIOC_STREAMON failed");

    while (!detected && frame_number < kWarmupFrames + kDetectionAttempts) {
        ESP_GOTO_ON_ERROR(wait_for_frame(fd), cleanup_stream, kTag,
                          "frame wait failed frame=%" PRIu32, frame_number);
        struct v4l2_buffer buffer = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
        };
        ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_DQBUF, &buffer) == 0, ESP_FAIL,
                          cleanup_stream, kTag, "VIDIOC_DQBUF failed");
        ESP_GOTO_ON_FALSE(buffer.index < kBufferCount, ESP_FAIL, cleanup_stream,
                          kTag, "invalid buffer index=%" PRIu32, buffer.index);
        ++frame_number;
        ESP_LOGI(kTag, "camera frame=%" PRIu32 " index=%" PRIu32 " bytes=%" PRIu32,
                 frame_number, buffer.index, buffer.bytesused);

        if (frame_number > kWarmupFrames) {
            // detector가 반환할 때까지 V4L2 buffer를 빌려 두어 frame lifetime을 보장한다.
            dl::image::img_t image = {
                .data = buffers[buffer.index].address,
                .width = static_cast<int>(format.fmt.pix.width),
                .height = static_cast<int>(format.fmt.pix.height),
                .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565,
            };
            const int64_t started_us = esp_timer_get_time();
            auto &results = detector->run(image);
            const int64_t elapsed_us = esp_timer_get_time() - started_us;
            ESP_LOGI(kTag, "camera inference_us=%lld result_count=%zu",
                     static_cast<long long>(elapsed_us), results.size());
            if (!results.empty()) {
                const auto largest = std::max_element(
                    results.begin(), results.end(),
                    [](const auto &left, const auto &right) {
                        return box_area(left) < box_area(right);
                    });
                detected = validate_and_log_result(*largest);
            }
            log_heap_snapshot("inference_done");
        }

        ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_QBUF, &buffer) == 0, ESP_FAIL,
                          cleanup_stream, kTag, "return QBUF index=%" PRIu32 " failed",
                          buffer.index);
    }

    ret = detected ? ESP_OK : ESP_ERR_NOT_FOUND;

cleanup_stream:
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) != 0) {
        ESP_LOGE(kTag, "VIDIOC_STREAMOFF failed");
        ret = ESP_FAIL;
    }
cleanup_detector:
    delete detector;
    log_heap_snapshot("detector_released");
cleanup:
    for (MappedBuffer &buffer : buffers) {
        if (buffer.address != nullptr && buffer.address != MAP_FAILED) {
            munmap(buffer.address, buffer.length);
        }
    }
    close(fd);
    log_heap_snapshot("camera_released");
    return ret;
}

} // namespace

extern "C" void app_main(void)
{
    ESP_LOGI(kTag, "JC1060P470C-I-W-Y C0.5 camera face probe start");
    vTaskDelay(pdMS_TO_TICKS(10000));
    ESP_LOGI(kTag, "monitor grace period complete; camera face probe begins");
    log_heap_snapshot("startup");

    esp_err_t status = initialize_camera();
    if (status == ESP_OK) {
        log_heap_snapshot("camera_initialized");
        status = capture_and_detect();
    }
    if (status == ESP_OK) {
        ESP_LOGI(kTag, "C0_5_CAMERA_FACE_PASS");
    } else {
        ESP_LOGE(kTag, "C0_5_CAMERA_FACE_FAIL error=%s", esp_err_to_name(status));
    }
}

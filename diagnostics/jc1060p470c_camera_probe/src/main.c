/*
 * SPDX-FileCopyrightText: 2026 HMI starter contributors
 * SPDX-License-Identifier: MIT
 */

#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "linux/videodev2.h"

static const char *TAG = "camera_probe";

enum {
    CAMERA_SCCB_PORT = 0,
    CAMERA_SCCB_SDA = 7,
    CAMERA_SCCB_SCL = 8,
    CAMERA_SCCB_FREQUENCY_HZ = 100000,
    CAMERA_BUFFER_COUNT = 2,
    CAMERA_FRAME_TARGET = 30,
    CAMERA_FRAME_TIMEOUT_SECONDS = 2,
};

typedef struct {
    void *address;
    size_t length;
} mapped_buffer_t;

static void fourcc_to_text(uint32_t fourcc, char text[5])
{
    text[0] = (char)(fourcc & 0xffU);
    text[1] = (char)((fourcc >> 8U) & 0xffU);
    text[2] = (char)((fourcc >> 16U) & 0xffU);
    text[3] = (char)((fourcc >> 24U) & 0xffU);
    text[4] = '\0';
}

static esp_err_t initialize_camera(void)
{
    const esp_video_init_csi_config_t csi_config = {
        .sccb_config = {
            .init_sccb = true,
            .i2c_config = {
                .port = CAMERA_SCCB_PORT,
                .scl_pin = CAMERA_SCCB_SCL,
                .sda_pin = CAMERA_SCCB_SDA,
            },
            .freq = CAMERA_SCCB_FREQUENCY_HZ,
        },
        .reset_pin = -1,
        .pwdn_pin = -1,
    };
    const esp_video_init_config_t video_config = {
        .csi = &csi_config,
    };

    ESP_LOGI(TAG, "init sensor=OV02C10 expected_sccb=0x36 sda=%d scl=%d freq=%d",
             CAMERA_SCCB_SDA, CAMERA_SCCB_SCL, CAMERA_SCCB_FREQUENCY_HZ);
    return esp_video_init(&video_config);
}

static esp_err_t wait_for_frame(int fd)
{
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    struct timeval timeout = {
        .tv_sec = CAMERA_FRAME_TIMEOUT_SECONDS,
        .tv_usec = 0,
    };
    const int result = select(fd + 1, &read_fds, NULL, NULL, &timeout);
    if (result == 0) {
        ESP_LOGE(TAG, "frame timeout seconds=%d", CAMERA_FRAME_TIMEOUT_SECONDS);
        return ESP_ERR_TIMEOUT;
    }
    if (result < 0) {
        ESP_LOGE(TAG, "select failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t capture_30_frames(void)
{
    esp_err_t ret = ESP_FAIL;
    mapped_buffer_t buffers[CAMERA_BUFFER_COUNT] = {0};
    int fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDONLY | O_NONBLOCK);
    ESP_RETURN_ON_FALSE(fd >= 0, ESP_FAIL, TAG, "open %s failed",
                        ESP_VIDEO_MIPI_CSI_DEVICE_NAME);

    struct v4l2_capability capability = {0};
    ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_QUERYCAP, &capability) == 0, ESP_FAIL,
                      cleanup, TAG, "VIDIOC_QUERYCAP failed");
    ESP_LOGI(TAG, "driver=%s card=%s bus=%s version=%" PRIu32,
             capability.driver, capability.card, capability.bus_info,
             capability.version);

    struct v4l2_format format = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .fmt.pix = {
            .width = 1288,
            .height = 728,
            .pixelformat = V4L2_PIX_FMT_SBGGR10,
        },
    };
    ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_S_FMT, &format) == 0, ESP_FAIL,
                      cleanup, TAG, "VIDIOC_S_FMT RAW10 1288x728 failed");

    char fourcc[5];
    fourcc_to_text(format.fmt.pix.pixelformat, fourcc);
    ESP_LOGI(TAG, "format width=%" PRIu32 " height=%" PRIu32
                  " fourcc=%s sizeimage=%" PRIu32,
             format.fmt.pix.width, format.fmt.pix.height, fourcc,
             format.fmt.pix.sizeimage);

    struct v4l2_requestbuffers request = {
        .count = CAMERA_BUFFER_COUNT,
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
    };
    ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_REQBUFS, &request) == 0, ESP_FAIL,
                      cleanup, TAG, "VIDIOC_REQBUFS failed");
    ESP_GOTO_ON_FALSE(request.count == CAMERA_BUFFER_COUNT, ESP_FAIL, cleanup,
                      TAG, "driver returned buffers=%" PRIu32 " expected=%d",
                      request.count, CAMERA_BUFFER_COUNT);

    for (uint32_t index = 0; index < CAMERA_BUFFER_COUNT; ++index) {
        struct v4l2_buffer buffer = {
            .index = index,
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
        };
        ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_QUERYBUF, &buffer) == 0, ESP_FAIL,
                          cleanup, TAG, "VIDIOC_QUERYBUF index=%" PRIu32 " failed", index);
        buffers[index].length = buffer.length;
        buffers[index].address = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE,
                                      MAP_SHARED, fd, buffer.m.offset);
        ESP_GOTO_ON_FALSE(buffers[index].address != MAP_FAILED, ESP_FAIL,
                          cleanup, TAG, "mmap index=%" PRIu32 " failed", index);
        ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_QBUF, &buffer) == 0, ESP_FAIL,
                          cleanup, TAG, "initial QBUF index=%" PRIu32 " failed", index);
        ESP_LOGI(TAG, "buffer index=%" PRIu32 " length=%zu queued=1",
                 index, buffers[index].length);
    }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_STREAMON, &type) == 0, ESP_FAIL,
                      cleanup, TAG, "VIDIOC_STREAMON failed");

    uint32_t captured = 0U;
    uint32_t returned = 0U;
    while (captured < CAMERA_FRAME_TARGET) {
        ESP_GOTO_ON_ERROR(wait_for_frame(fd), cleanup_stream, TAG,
                          "frame wait failed captured=%" PRIu32, captured);
        struct v4l2_buffer buffer = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
        };
        ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_DQBUF, &buffer) == 0, ESP_FAIL,
                          cleanup_stream, TAG, "VIDIOC_DQBUF failed");
        ESP_GOTO_ON_FALSE(buffer.index < CAMERA_BUFFER_COUNT, ESP_FAIL,
                          cleanup_stream, TAG, "invalid buffer index=%" PRIu32,
                          buffer.index);

        ++captured;
        ESP_LOGI(TAG, "frame=%" PRIu32 "/%d index=%" PRIu32
                      " bytes=%" PRIu32 " flags=0x%08" PRIx32,
                 captured, CAMERA_FRAME_TARGET, buffer.index, buffer.bytesused,
                 buffer.flags);

        ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_QBUF, &buffer) == 0, ESP_FAIL,
                          cleanup_stream, TAG, "return QBUF index=%" PRIu32 " failed",
                          buffer.index);
        ++returned;
    }
    ret = ESP_OK;

cleanup_stream:
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) != 0) {
        ESP_LOGE(TAG, "VIDIOC_STREAMOFF failed");
        ret = ESP_FAIL;
    }
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "PASS captured=%" PRIu32 " returned=%" PRIu32
                      " outstanding=%" PRIu32,
                 captured, returned, captured - returned);
    }
cleanup:
    for (uint32_t index = 0; index < CAMERA_BUFFER_COUNT; ++index) {
        if (buffers[index].address != NULL && buffers[index].address != MAP_FAILED) {
            munmap(buffers[index].address, buffers[index].length);
        }
    }
    close(fd);
    return ret;
}

void app_main(void)
{
    ESP_LOGI(TAG, "JC1060P470C-I-W-Y C0 camera probe start");
    esp_err_t result = initialize_camera();
    if (result == ESP_OK) {
        result = capture_30_frames();
    }
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "C0_CAMERA_PROBE_PASS");
    } else {
        ESP_LOGE(TAG, "C0_CAMERA_PROBE_FAIL error=%s", esp_err_to_name(result));
    }
}

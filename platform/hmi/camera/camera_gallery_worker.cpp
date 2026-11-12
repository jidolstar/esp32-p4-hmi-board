#include "camera/camera_gallery_worker.hpp"

#include <utility>

#include <esp_heap_caps.h>

#include "log.hpp"

namespace hmi::platform::hmi_host::camera {
namespace {
using namespace ::hmi::camera;
using ::hmi::boards::jc1060p470c::CameraStartResult;

constexpr std::uint32_t kWorkerStackBytes = 8192U;
constexpr std::uint32_t kCaptureTimeoutMs = 600U;
constexpr auto kPreviewReportTicks = pdMS_TO_TICKS(10000U);

void log_camera_memory(const char* phase) noexcept {
  HMI_LOG_INFO("[camera] %s psram_free=%u dma_internal_largest=%u worker_stack_remaining=%u\r\n",
               phase, static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
               static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)),
               static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
}

CameraError camera_error(CameraStartResult result) noexcept {
  switch (result) {
    case CameraStartResult::ready: return CameraError::none;
    case CameraStartResult::probe_failed: return CameraError::probe_failed;
    case CameraStartResult::out_of_memory: return CameraError::out_of_memory;
    case CameraStartResult::stream_failed: return CameraError::stream_failed;
  }
  return CameraError::stream_failed;
}
}  // namespace

using namespace ::hmi::camera;

bool CameraGalleryWorker::begin() noexcept {
  // 시작과 해제에서 같은 CPU를 사용해 CSI와 JPEG interrupt 소유권을 유지한다.
  return task_ || xTaskCreatePinnedToCore(entry, "camera_gallery", kWorkerStackBytes,
                                          this, 2U, &task_, 0) == pdPASS;
}

CameraGalleryWorker::~CameraGalleryWorker() {
  cancel_and_drain();
  stop();
  drain(closing_);
}

bool CameraGalleryWorker::post(CameraCommand command) noexcept {
  portENTER_CRITICAL(&mux_);
  const bool accepted = task_ && !camera_busy_ && !stopping_ && !closing_ &&
                        camera_done_.token == 0U && command.token != 0U;
  if (accepted) {
    camera_busy_ = true;
    camera_command_ = command;
  }
  portEXIT_CRITICAL(&mux_);
  if (accepted) {
    xTaskNotifyGive(task_);
  }
  return accepted;
}

bool CameraGalleryWorker::post(FileCommand command) noexcept {
  portENTER_CRITICAL(&mux_);
  const bool accepted = task_ && !file_busy_ && !cancelling_ && !closing_ &&
                        file_done_.token == 0U && command.token != 0U;
  if (accepted) {
    file_busy_ = true;
    file_command_ = std::move(command);
  }
  portEXIT_CRITICAL(&mux_);
  if (accepted) {
    xTaskNotifyGive(task_);
  }
  return accepted;
}

bool CameraGalleryWorker::request_start(std::uint32_t token) noexcept {
  return post(CameraCommand{CameraOp::start, token});
}

bool CameraGalleryWorker::request_capture(std::uint32_t token) noexcept {
  return preview_ready_.load(std::memory_order_acquire) && post(CameraCommand{CameraOp::capture, token});
}

bool CameraGalleryWorker::request_scan(std::uint32_t token, std::size_t page_index) noexcept {
  return post(FileCommand{FileOp::scan, token, 0U, page_index, {}});
}

bool CameraGalleryWorker::request_save(std::uint32_t token, CameraFrameLease frame) noexcept {
  return frame && post(FileCommand{FileOp::save, token, 0U, 0U, std::move(frame)});
}

bool CameraGalleryWorker::request_open(std::uint32_t token, PhotoId id) noexcept {
  return id != 0U && post(FileCommand{FileOp::open, token, id, 0U, {}});
}

bool CameraGalleryWorker::request_delete(std::uint32_t token, PhotoId id) noexcept {
  return id != 0U && post(FileCommand{FileOp::erase, token, id, 0U, {}});
}

bool CameraGalleryWorker::take_completion(CameraCompletion& output) noexcept {
  CameraCompletion result;
  portENTER_CRITICAL(&mux_);
  result = std::exchange(camera_done_, {});
  portEXIT_CRITICAL(&mux_);
  if (result.token == 0U) {
    return false;
  }
  output = std::move(result);
  return true;
}

bool CameraGalleryWorker::take_completion(GalleryCompletion& output) noexcept {
  GalleryCompletion result;
  portENTER_CRITICAL(&mux_);
  result = std::exchange(file_done_, {});
  portEXIT_CRITICAL(&mux_);
  if (result.token == 0U) {
    return false;
  }
  output = std::move(result);
  return true;
}

CameraFrameLease CameraGalleryWorker::acquire_preview() noexcept {
  auto frame = acquire_frame();
  preview_samples_ += static_cast<std::uint32_t>(static_cast<bool>(frame));
  const auto now = xTaskGetTickCount();
  if (preview_since_ == 0U) preview_since_ = now;
  const auto elapsed = now - preview_since_;
  if (elapsed >= kPreviewReportTicks) {
    const auto rate = static_cast<unsigned>(static_cast<std::uint64_t>(preview_samples_) *
        configTICK_RATE_HZ * 10U / elapsed);
    HMI_LOG_INFO("[camera] preview fps=%u.%u frames=%u\r\n", rate / 10U, rate % 10U,
                 static_cast<unsigned>(preview_samples_));
    preview_since_ = now;
    preview_samples_ = 0U;
  }
  return frame;
}

CameraFrameLease CameraGalleryWorker::acquire_frame() noexcept {
  if (!preview_ready_.load(std::memory_order_acquire)) {
    return {};
  }
  const auto frame = camera_.borrow_frame();
  if (!frame.pixels) {
    return {};
  }
  return {{frame.pixels, frame.bytes, frame.id, frame.width, frame.height, frame.stride_bytes},
          &camera_, return_frame};
}

void CameraGalleryWorker::return_frame(void* context, std::uint32_t id) noexcept {
  static_cast<::hmi::boards::jc1060p470c::Camera*>(context)->return_frame(id);
}

void CameraGalleryWorker::drain(bool& flag) noexcept {
  portENTER_CRITICAL(&mux_);
  const auto task = task_;
  if (task) {
    flag = true;
  }
  portEXIT_CRITICAL(&mux_);
  if (!task) {
    return;
  }
  xTaskNotifyGive(task);
  for (;;) {
    portENTER_CRITICAL(&mux_);
    const bool pending = flag;
    portEXIT_CRITICAL(&mux_);
    if (!pending) {
      return;
    }
    vTaskDelay(1U);
  }
}

void CameraGalleryWorker::stop() noexcept {
  preview_since_ = 0U;
  preview_samples_ = 0U;
  preview_ready_.store(false, std::memory_order_release);
  drain(stopping_);
}

void CameraGalleryWorker::cancel_and_drain() noexcept { drain(cancelling_); }

void CameraGalleryWorker::run_camera() noexcept {
  CameraCompletion result;
  portENTER_CRITICAL(&mux_);
  const bool stopping = stopping_;
  const auto command = std::exchange(camera_command_, {});
  if (stopping) {
    result = std::exchange(camera_done_, {});
  }
  portEXIT_CRITICAL(&mux_);
  if (stopping) {
    result.frame.reset();
    preview_ready_.store(false, std::memory_order_release);
    if (!camera_.stop()) {
      HMI_LOG_ERROR("[camera] stop failed\r\n");
    }
    log_camera_memory("preview_stopped");
    portENTER_CRITICAL(&mux_);
    stopping_ = false;
    camera_busy_ = false;
    portEXIT_CRITICAL(&mux_);
    return;
  }
  if (command.op == CameraOp::none) {
    return;
  }
  result.token = command.token;
  if (command.op == CameraOp::start) {
    result.error = camera_error(camera_.start());
    preview_ready_.store(result.error == CameraError::none, std::memory_order_release);
    if (result.error == CameraError::none) log_camera_memory("preview_started");
  } else {
    const auto start = xTaskGetTickCount();
    do {
      result.frame = acquire_frame();
      if (result.frame) {
        break;
      }
      vTaskDelay(1U);
    } while (xTaskGetTickCount() - start < pdMS_TO_TICKS(kCaptureTimeoutMs));
    if (!result.frame) {
      result.error = CameraError::frame_timeout;
    }
  }
  portENTER_CRITICAL(&mux_);
  camera_done_ = std::move(result);
  camera_busy_ = false;
  portEXIT_CRITICAL(&mux_);
}

void CameraGalleryWorker::run_files() noexcept {
  GalleryCompletion result;
  portENTER_CRITICAL(&mux_);
  const bool cancelling = cancelling_;
  auto command = std::exchange(file_command_, {});
  if (cancelling) {
    result = std::exchange(file_done_, {});
  }
  portEXIT_CRITICAL(&mux_);
  if (cancelling) {
    command.frame.reset();
    result.image.reset();
    portENTER_CRITICAL(&mux_);
    cancelling_ = false;
    file_busy_ = false;
    portEXIT_CRITICAL(&mux_);
    return;
  }
  if (command.op == FileOp::none) {
    return;
  }
  result.token = command.token;
  switch (command.op) {
    case FileOp::scan: result.error = files_.scan(command.page, result); break;
    case FileOp::save: result.error = jpeg_.save(std::move(command.frame), result.saved_id); break;
    case FileOp::open: result.error = jpeg_.open(command.id, result.image); break;
    case FileOp::erase: result.error = files_.erase(command.id); break;
    case FileOp::none: break;
  }
  portENTER_CRITICAL(&mux_);
  file_done_ = std::move(result);
  file_busy_ = false;
  portEXIT_CRITICAL(&mux_);
}

void CameraGalleryWorker::entry(void* context) { static_cast<CameraGalleryWorker*>(context)->run(); }

void CameraGalleryWorker::run() noexcept {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    run_files();
    run_camera();
    portENTER_CRITICAL(&mux_);
    const bool done = closing_;
    if (done) {
      task_ = nullptr;
      closing_ = false;
    }
    portEXIT_CRITICAL(&mux_);
    if (done) {
      vTaskDelete(nullptr);
      return;
    }
  }
}

}  // namespace hmi::platform::hmi_host::camera

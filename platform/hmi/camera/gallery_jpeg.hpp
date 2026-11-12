#pragma once

#include "camera/gallery_sd_files.hpp"

namespace hmi::platform::hmi_host::camera {

/** JPEG 하드웨어와 PSRAM을 사용한다. SD·JPEG 작업을 직렬 실행하는 worker가 호출한다. */
class GalleryJpeg {
 public:
  explicit GalleryJpeg(GallerySdFiles& files) noexcept : files_(files) {}

  ::hmi::camera::GalleryError save(::hmi::camera::CameraFrameLease frame,
                                   ::hmi::camera::PhotoId& saved_id) noexcept;
  ::hmi::camera::GalleryError open(::hmi::camera::PhotoId id,
                                   ::hmi::camera::CameraFrameLease& image) noexcept;

 private:
  GallerySdFiles& files_;
};

}  // namespace hmi::platform::hmi_host::camera

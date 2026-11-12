#include "hmi/update/artifact_download.hpp"

#include <cstring>

namespace hmi::update {
namespace {

DownloadIntent current_intent(const DownloadControl& control) noexcept {
  return control.intent == nullptr ? DownloadIntent::continue_download
                                   : control.intent(control.context);
}

void publish_progress(const DownloadControl& control, std::uint64_t completed,
                      std::uint64_t total) noexcept {
  if (control.progress != nullptr) {
    control.progress(control.context, completed, total);
  }
}

class DownloadClose {
 public:
  explicit DownloadClose(DownloadPort& port) noexcept : port_(port) {}
  ~DownloadClose() {
    if (open_ && port_.close != nullptr) {
      port_.close(port_.context);
    }
  }
  void mark_open() noexcept { open_ = true; }

 private:
  DownloadPort& port_;
  bool open_ = false;
};

}  // namespace

Error ArtifactDownloader::digest_existing(const Manifest& manifest,
                                          std::uint64_t bytes,
                                          std::uint8_t* scratch,
                                          std::size_t scratch_size) noexcept {
  std::uint64_t offset = 0U;
  while (offset < bytes) {
    const auto remaining = bytes - offset;
    const auto requested = remaining < scratch_size
                               ? static_cast<std::size_t>(remaining)
                               : scratch_size;
    std::size_t used = 0U;
    const auto result = staging_.read_partial(
        staging_.context, manifest.installation_id, offset, scratch, requested,
        used);
    if (result != Error::none || used == 0U || used > requested ||
        !crypto_.digest_update(crypto_.context, scratch, used)) {
      return Error::storage;
    }
    offset += used;
  }
  return Error::none;
}

Error ArtifactDownloader::run(const Manifest& manifest, DownloadControl control,
                              std::uint8_t* scratch,
                              std::size_t scratch_size) noexcept {
  if (manifest.target == Target::invalid || manifest.size == 0U ||
      manifest.size > kMaximumArtifactBytes ||
      !valid_uuid(manifest.installation_id) ||
      !valid_sha256(manifest.sha256) ||
      !safe_relative_path(manifest.artifact_path) || scratch == nullptr ||
      scratch_size == 0U || scratch_size > kMaximumIoChunkBytes ||
      download_.open == nullptr || download_.read == nullptr ||
      download_.close == nullptr || staging_.partial_size == nullptr ||
      staging_.read_partial == nullptr ||
      staging_.append_partial == nullptr ||
      staging_.truncate_partial == nullptr || staging_.commit == nullptr ||
      staging_.remove_partial == nullptr || crypto_.digest_begin == nullptr ||
      crypto_.digest_update == nullptr || crypto_.digest_finish == nullptr ||
      crypto_.digest_abort == nullptr) {
    return Error::invalid_argument;
  }

  std::uint64_t offset = 0U;
  auto result = staging_.partial_size(staging_.context,
                                      manifest.installation_id, offset);
  if (result != Error::none) {
    return result;
  }
  if (offset > manifest.size) {
    result = staging_.truncate_partial(staging_.context,
                                       manifest.installation_id, 0U);
    if (result != Error::none) {
      return result;
    }
    offset = 0U;
  }
  if (!crypto_.digest_begin(crypto_.context)) {
    return Error::unavailable;
  }
  if (offset > 0U) {
    result = digest_existing(manifest, offset, scratch, scratch_size);
    if (result != Error::none) {
      crypto_.digest_abort(crypto_.context);
      return result;
    }
  }

  DownloadClose close(download_);
  result = download_.open(download_.context, manifest.artifact_path, offset,
                          manifest.size);
  if (result != Error::none) {
    crypto_.digest_abort(crypto_.context);
    return result;
  }
  close.mark_open();
  publish_progress(control, offset, manifest.size);

  while (offset < manifest.size) {
    const auto intent = current_intent(control);
    if (intent == DownloadIntent::pause) {
      crypto_.digest_abort(crypto_.context);
      return Error::paused;
    }
    if (intent == DownloadIntent::cancel) {
      crypto_.digest_abort(crypto_.context);
      const auto remove = staging_.remove_partial(
          staging_.context, manifest.installation_id);
      return remove == Error::none ? Error::cancelled : remove;
    }
    const auto remaining = manifest.size - offset;
    const auto capacity = remaining < scratch_size
                              ? static_cast<std::size_t>(remaining)
                              : scratch_size;
    std::size_t used = 0U;
    bool complete = false;
    result = download_.read(download_.context, scratch, capacity, used, complete);
    if (result != Error::none) {
      crypto_.digest_abort(crypto_.context);
      return result;
    }
    if (used == 0U || used > capacity || (complete && used != remaining)) {
      crypto_.digest_abort(crypto_.context);
      return Error::network;
    }
    result = staging_.append_partial(staging_.context,
                                     manifest.installation_id, scratch, used);
    if (result != Error::none ||
        !crypto_.digest_update(crypto_.context, scratch, used)) {
      crypto_.digest_abort(crypto_.context);
      return result == Error::none ? Error::unavailable : result;
    }
    offset += used;
    publish_progress(control, offset, manifest.size);
  }

  char digest[65]{};
  if (!crypto_.digest_finish(crypto_.context, digest)) {
    crypto_.digest_abort(crypto_.context);
    return Error::unavailable;
  }
  if (std::strcmp(digest, manifest.sha256) != 0) {
    const auto remove = staging_.remove_partial(staging_.context,
                                                manifest.installation_id);
    return remove == Error::none ? Error::digest : remove;
  }
  return staging_.commit(staging_.context, manifest.installation_id);
}

}  // namespace hmi::update

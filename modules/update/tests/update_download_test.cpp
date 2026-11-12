#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

#include "hmi/update/artifact_download.hpp"

namespace {

constexpr char kId[] = "11111111-1111-1111-1111-111111111111";
constexpr char kSha[] =
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

struct Fixture {
  std::vector<std::uint8_t> remote{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};
  std::vector<std::uint8_t> partial{};
  std::vector<std::uint8_t> digested{};
  std::uint64_t opened_at = 0U;
  bool opened = false;
  bool closed = false;
  bool committed = false;
  bool removed = false;
  bool zero_read = false;
  const char* digest = kSha;
  hmi::update::DownloadIntent next_intent =
      hmi::update::DownloadIntent::continue_download;
  std::uint64_t last_progress = 0U;
};

hmi::update::Error open(void* context, std::string_view path,
                        std::uint64_t offset, std::uint64_t expected) {
  auto& fixture = *static_cast<Fixture*>(context);
  if (path != "firmware/release.bin" || expected != fixture.remote.size() ||
      offset > expected) {
    return hmi::update::Error::network;
  }
  fixture.opened = true;
  fixture.opened_at = offset;
  return hmi::update::Error::none;
}

hmi::update::Error read(void* context, std::uint8_t* output,
                        std::size_t capacity, std::size_t& used,
                        bool& complete) {
  auto& fixture = *static_cast<Fixture*>(context);
  if (fixture.zero_read) {
    used = 0U;
    complete = false;
    return hmi::update::Error::none;
  }
  const auto offset = fixture.opened_at;
  const auto count = std::min(capacity, fixture.remote.size() - offset);
  std::memcpy(output, fixture.remote.data() + offset, count);
  fixture.opened_at += count;
  used = count;
  complete = fixture.opened_at == fixture.remote.size();
  return hmi::update::Error::none;
}

void close(void* context) { static_cast<Fixture*>(context)->closed = true; }

hmi::update::Error partial_size(void* context, std::string_view,
                                std::uint64_t& output) {
  output = static_cast<Fixture*>(context)->partial.size();
  return hmi::update::Error::none;
}

hmi::update::Error read_partial(void* context, std::string_view,
                                std::uint64_t offset, std::uint8_t* output,
                                std::size_t capacity, std::size_t& used) {
  auto& value = static_cast<Fixture*>(context)->partial;
  if (offset >= value.size()) {
    used = 0U;
    return hmi::update::Error::storage;
  }
  used = std::min(capacity, value.size() - static_cast<std::size_t>(offset));
  std::memcpy(output, value.data() + offset, used);
  return hmi::update::Error::none;
}

hmi::update::Error append(void* context, std::string_view,
                          const std::uint8_t* data, std::size_t size) {
  auto& value = static_cast<Fixture*>(context)->partial;
  value.insert(value.end(), data, data + size);
  return hmi::update::Error::none;
}

hmi::update::Error truncate(void* context, std::string_view,
                            std::uint64_t size) {
  static_cast<Fixture*>(context)->partial.resize(static_cast<std::size_t>(size));
  return hmi::update::Error::none;
}

hmi::update::Error commit(void* context, std::string_view) {
  static_cast<Fixture*>(context)->committed = true;
  return hmi::update::Error::none;
}

hmi::update::Error remove(void* context, std::string_view) {
  auto& fixture = *static_cast<Fixture*>(context);
  fixture.partial.clear();
  fixture.removed = true;
  return hmi::update::Error::none;
}

bool digest_begin(void* context) {
  static_cast<Fixture*>(context)->digested.clear();
  return true;
}

bool digest_update(void* context, const std::uint8_t* data, std::size_t size) {
  auto& value = static_cast<Fixture*>(context)->digested;
  value.insert(value.end(), data, data + size);
  return true;
}

bool digest_finish(void* context, char (&output)[65]) {
  std::strcpy(output, static_cast<Fixture*>(context)->digest);
  return true;
}

void digest_abort(void*) {}

hmi::update::DownloadIntent intent(void* context) {
  return static_cast<Fixture*>(context)->next_intent;
}

void progress(void* context, std::uint64_t completed, std::uint64_t) {
  static_cast<Fixture*>(context)->last_progress = completed;
}

hmi::update::Manifest manifest() {
  hmi::update::Manifest value{};
  std::strcpy(value.installation_id, kId);
  std::strcpy(value.device_id, "22222222-2222-2222-2222-222222222222");
  std::strcpy(value.version, "1.2.3");
  std::strcpy(value.sha256, kSha);
  std::strcpy(value.artifact_path, "firmware/release.bin");
  value.size = 8U;
  value.expires_at = 1704070800;
  value.target = hmi::update::Target::firmware;
  return value;
}

hmi::update::ArtifactDownloader downloader(Fixture& fixture) {
  hmi::update::DownloadPort download{&fixture, &open, &read, &close};
  hmi::update::StagingPort staging{&fixture, &partial_size, &read_partial,
                                   &append, &truncate, &commit, &remove};
  hmi::update::CryptoPort crypto{};
  crypto.context = &fixture;
  crypto.digest_begin = &digest_begin;
  crypto.digest_update = &digest_update;
  crypto.digest_finish = &digest_finish;
  crypto.digest_abort = &digest_abort;
  return {download, staging, crypto};
}

hmi::update::DownloadControl control(Fixture& fixture) {
  return {&fixture, &intent, &progress};
}

}  // namespace

int main() {
  using namespace hmi::update;
  std::uint8_t scratch[3]{};

  Fixture fresh;
  auto fresh_downloader = downloader(fresh);
  assert(fresh_downloader.run(manifest(), control(fresh), scratch,
                              sizeof(scratch)) == Error::none);
  assert(fresh.partial == fresh.remote);
  assert(fresh.digested == fresh.remote);
  assert(fresh.committed && fresh.closed);
  assert(fresh.last_progress == fresh.remote.size());

  Fixture resumed;
  resumed.partial.assign(resumed.remote.begin(), resumed.remote.begin() + 3);
  auto resumed_downloader = downloader(resumed);
  assert(resumed_downloader.run(manifest(), control(resumed), scratch,
                                sizeof(scratch)) == Error::none);
  assert(resumed.opened_at == resumed.remote.size());
  assert(resumed.partial == resumed.remote);
  assert(resumed.digested == resumed.remote);

  Fixture paused;
  paused.next_intent = DownloadIntent::pause;
  auto paused_downloader = downloader(paused);
  assert(paused_downloader.run(manifest(), control(paused), scratch,
                               sizeof(scratch)) == Error::paused);
  assert(!paused.removed);

  Fixture cancelled;
  cancelled.partial = {1U, 2U};
  cancelled.next_intent = DownloadIntent::cancel;
  auto cancelled_downloader = downloader(cancelled);
  assert(cancelled_downloader.run(manifest(), control(cancelled), scratch,
                                  sizeof(scratch)) == Error::cancelled);
  assert(cancelled.removed && cancelled.partial.empty());

  Fixture mismatch;
  mismatch.digest =
      "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
  auto mismatch_downloader = downloader(mismatch);
  assert(mismatch_downloader.run(manifest(), control(mismatch), scratch,
                                 sizeof(scratch)) == Error::digest);
  assert(mismatch.removed && !mismatch.committed);

  Fixture short_read;
  short_read.zero_read = true;
  auto short_downloader = downloader(short_read);
  assert(short_downloader.run(manifest(), control(short_read), scratch,
                              sizeof(scratch)) == Error::network);

  Fixture invalid;
  auto invalid_downloader = downloader(invalid);
  std::uint8_t too_large[kMaximumIoChunkBytes + 1U]{};
  assert(invalid_downloader.run(manifest(), control(invalid), too_large,
                                sizeof(too_large)) == Error::invalid_argument);

  return 0;
}

#include "audio.hpp"

#include "internal/audio_service.hpp"

namespace hmi::boards::jc1060p470c {

bool Audio::play(const char* path) { return service_->play(path); }
bool Audio::pause() { return service_->pause(); }
bool Audio::resume() { return service_->resume(); }
bool Audio::stop() { return service_->stop(); }
bool Audio::set_volume(std::uint8_t volume) {
  return service_->set_volume(volume);
}
AudioStatus Audio::status() const noexcept { return service_->status(); }

AudioProbeResult Audio::probe(const char* path, AudioFileInfo& output) const {
  return service_->probe(path, output);
}

}  // namespace hmi::boards::jc1060p470c

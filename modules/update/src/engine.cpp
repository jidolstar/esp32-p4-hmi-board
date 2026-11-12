#include "hmi/update/engine.hpp"

namespace hmi::update {

Error Engine::prepare(const Manifest& manifest) noexcept {
  if (!gate_.try_enter()) {
    return Error::busy;
  }
  auto result = journal_.begin(manifest.installation_id);
  if (result == Error::none) {
    result = assets_.prepare(manifest.installation_id);
  }
  if (result != Error::none) {
    if (journal_.active()) {
      const auto ignored = journal_.transition(InstallStage::failed, result);
      (void)ignored;
    }
    gate_.leave();
  }
  return result;
}

Error Engine::mark_download_started() noexcept {
  return journal_.transition(InstallStage::downloading);
}

Error Engine::mark_verified() noexcept {
  return journal_.transition(InstallStage::verified);
}

Error Engine::mark_installing() noexcept {
  return journal_.transition(InstallStage::installing);
}

Error Engine::mark_awaiting_boot() noexcept {
  return journal_.transition(InstallStage::awaiting_boot);
}

Error Engine::confirm_boot() noexcept {
  const auto generations = assets_.snapshot();
  const auto journal = journal_.snapshot();
  auto result = generations.pending[0] != '\0'
                    ? assets_.activate_pending()
                    : (std::string_view(generations.active) ==
                               journal.installation_id
                           ? Error::none
                           : Error::binding);
  if (result == Error::none) {
    result = journal_.transition(InstallStage::confirmed);
  }
  if (result == Error::none) {
    gate_.leave();
  }
  return result;
}

Error Engine::request_rollback(Error reason) noexcept {
  if (reason == Error::none) {
    return Error::invalid_argument;
  }
  return journal_.transition(InstallStage::rolling_back, reason);
}

Error Engine::complete_rollback() noexcept {
  const auto generations = assets_.snapshot();
  auto result = generations.pending[0] != '\0'
                    ? assets_.discard_pending()
                    : Error::none;
  if (result == Error::none) {
    result = journal_.transition(InstallStage::rolled_back);
  }
  if (result == Error::none) {
    gate_.leave();
  }
  return result;
}

Error Engine::fail(Error reason) noexcept {
  if (reason == Error::none) {
    return Error::invalid_argument;
  }
  const auto result = journal_.transition(InstallStage::failed, reason);
  if (result == Error::none) {
    gate_.leave();
  }
  return result;
}

}  // namespace hmi::update

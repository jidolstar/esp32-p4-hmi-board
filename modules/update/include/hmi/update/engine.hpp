#pragma once

#include "hmi/update/asset_lifecycle.hpp"
#include "hmi/update/install_journal.hpp"
#include "hmi/update/maintenance_gate.hpp"
#include "hmi/update/types.hpp"

namespace hmi::update {

/**
 * update core 단계만 조정하는 작은 engine이다.
 *
 * network, partition write와 reboot를 호출하지 않는다. adapter가 작업을 마친 뒤 명시적인
 * mark 메서드를 호출해야 다음 durable stage로 이동한다.
 */
class Engine {
 public:
  Engine(MaintenanceGate& gate, InstallJournal& journal,
         AssetLifecycle& assets) noexcept
      : gate_(gate), journal_(journal), assets_(assets) {}

  [[nodiscard]] Error prepare(const Manifest& manifest) noexcept;
  [[nodiscard]] Error mark_download_started() noexcept;
  [[nodiscard]] Error mark_verified() noexcept;
  [[nodiscard]] Error mark_installing() noexcept;
  [[nodiscard]] Error mark_awaiting_boot() noexcept;
  [[nodiscard]] Error confirm_boot() noexcept;
  [[nodiscard]] Error request_rollback(Error reason) noexcept;
  [[nodiscard]] Error complete_rollback() noexcept;
  [[nodiscard]] Error fail(Error reason) noexcept;

 private:
  MaintenanceGate& gate_;
  InstallJournal& journal_;
  AssetLifecycle& assets_;
};

static_assert(sizeof(Engine) <= 24U,
              "update engine must keep only three non-owning references");

}  // namespace hmi::update

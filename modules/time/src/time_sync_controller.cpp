#include "hmi/time/time_sync_controller.hpp"

#include <limits>

namespace hmi::time {
namespace {

constexpr std::uint8_t kMaximumFailureCount =
    std::numeric_limits<std::uint8_t>::max();

std::uint32_t elapsed(std::uint32_t now_ms,
                      std::uint32_t started_ms) noexcept {
  return now_ms - started_ms;
}

std::uint64_t seconds_from_2000(const DateTimeValue& value) noexcept {
  std::uint64_t days = 0U;
  for (std::uint16_t year = 2000U; year < value.year; ++year) {
    days += is_leap_year(year) ? 366U : 365U;
  }
  for (std::uint8_t month = 1U; month < value.month; ++month) {
    days += days_in_month(value.year, month);
  }
  days += static_cast<std::uint64_t>(value.day - 1U);
  return days * 24U * 60U * 60U +
         static_cast<std::uint64_t>(value.hour) * 60U * 60U +
         static_cast<std::uint64_t>(value.minute) * 60U + value.second;
}

std::uint32_t difference_seconds(const DateTimeValue& left,
                                 const DateTimeValue& right) noexcept {
  const std::uint64_t left_seconds = seconds_from_2000(left);
  const std::uint64_t right_seconds = seconds_from_2000(right);
  const std::uint64_t difference = left_seconds >= right_seconds
                                       ? left_seconds - right_seconds
                                       : right_seconds - left_seconds;
  return difference > std::numeric_limits<std::uint32_t>::max()
             ? std::numeric_limits<std::uint32_t>::max()
             : static_cast<std::uint32_t>(difference);
}

bool required_ports_ready(const TimeSyncPorts& ports) noexcept {
  return ports.system.set_local != nullptr &&
         ports.system.read_local != nullptr &&
         ports.network.request_sync != nullptr &&
         ports.network.sync_generation != nullptr &&
         ports.connectivity.has_ip != nullptr;
}

bool valid_policy(const TimeSyncPolicy& policy) noexcept {
  return policy.sync_timeout_ms != 0U &&
         policy.regular_sync_interval_ms != 0U &&
         policy.first_retry_ms != 0U && policy.second_retry_ms != 0U &&
         policy.third_retry_ms != 0U && policy.maximum_retry_ms != 0U;
}

}  // namespace

bool TimeSyncController::begin(const TimeSyncPorts& ports,
                               std::uint32_t now_ms,
                               const TimeSyncPolicy& policy) noexcept {
  *this = TimeSyncController{};
  if (!required_ports_ready(ports) || !valid_policy(policy)) {
    last_error_ = TimeSyncError::invalid_ports;
    return false;
  }

  ports_ = ports;
  policy_ = policy;
  initialized_ = true;
  current_ms_ = now_ms;
  observed_generation_ =
      ports_.network.sync_generation(ports_.network.context);

  DateTimeValue rtc_value{};
  if (ports_.rtc.read != nullptr) {
    rtc_status_ = ports_.rtc.read(ports_.rtc.context, rtc_value);
    if (rtc_status_ == RtcStatus::ready && is_valid(rtc_value)) {
      publish(rtc_value, TimeSource::rtc, now_ms);
      if (!ports_.system.set_local(ports_.system.context, rtc_value)) {
        last_error_ = TimeSyncError::system_write;
      }
    } else {
      if (rtc_status_ == RtcStatus::ready) {
        rtc_status_ = RtcStatus::invalid;
      }
      if (rtc_status_ == RtcStatus::read_error) {
        last_error_ = TimeSyncError::rtc_read;
      }
    }
  }

  had_ip_ = ports_.connectivity.has_ip(ports_.connectivity.context);
  if (had_ip_) {
    request_network_sync(now_ms);
  } else {
    network_state_ = NetworkState::waiting_for_ip;
  }
  return true;
}

void TimeSyncController::tick(std::uint32_t now_ms) noexcept {
  if (!initialized_) {
    return;
  }
  current_ms_ = now_ms;
  if (consume_network_sync(now_ms)) {
    return;
  }

  const bool has_ip =
      ports_.connectivity.has_ip(ports_.connectivity.context);
  if (!has_ip) {
    had_ip_ = false;
    network_state_ = NetworkState::waiting_for_ip;
    if (has_last_success_ &&
        elapsed(now_ms, last_success_ms_) >=
            policy_.regular_sync_interval_ms) {
      sync_due_ = true;
    }
    return;
  }

  if (!had_ip_) {
    had_ip_ = true;
    request_network_sync(now_ms);
    return;
  }
  if (network_state_ == NetworkState::syncing &&
      elapsed(now_ms, attempt_started_ms_) >= policy_.sync_timeout_ms) {
    mark_network_failure(now_ms, TimeSyncError::network_timeout);
    return;
  }
  if (has_last_success_ &&
      elapsed(now_ms, last_success_ms_) >=
          policy_.regular_sync_interval_ms) {
    sync_due_ = true;
  }
  if (retry_scheduled_) {
    if (elapsed(now_ms, retry_started_ms_) >= retry_delay()) {
      request_network_sync(now_ms);
    }
    return;
  }
  if (sync_due_ && network_state_ != NetworkState::syncing) {
    request_network_sync(now_ms);
  }
}

ManualApplyResult TimeSyncController::apply_manual(
    const DateTimeValue& value) noexcept {
  return apply_manual(value, current_ms_);
}

ManualApplyResult TimeSyncController::apply_manual(
    const DateTimeValue& value, std::uint32_t now_ms) noexcept {
  if (!initialized_ || !is_valid(value)) {
    last_error_ = TimeSyncError::invalid_manual_value;
    return ManualApplyResult::invalid_value;
  }
  if (!ports_.system.set_local(ports_.system.context, value)) {
    last_error_ = TimeSyncError::system_write;
    return ManualApplyResult::system_write_failed;
  }

  publish(value, TimeSource::manual, now_ms);
  has_last_rtc_drift_ = false;
  if (ports_.rtc.write == nullptr) {
    rtc_status_ = RtcStatus::not_detected;
    last_error_ = TimeSyncError::rtc_write;
    return ManualApplyResult::applied_rtc_write_failed;
  }

  rtc_status_ = ports_.rtc.write(ports_.rtc.context, value);
  if (rtc_status_ != RtcStatus::ready) {
    if (rtc_status_ != RtcStatus::not_detected) {
      rtc_status_ = RtcStatus::write_error;
    }
    last_error_ = TimeSyncError::rtc_write;
    return ManualApplyResult::applied_rtc_write_failed;
  }
  last_error_ = TimeSyncError::none;
  return ManualApplyResult::applied;
}

TimeSyncSnapshot TimeSyncController::snapshot() const noexcept {
  return snapshot(current_ms_);
}

TimeSyncSnapshot TimeSyncController::snapshot(
    std::uint32_t now_ms) const noexcept {
  TimeSyncSnapshot result{};
  result.source = source_;
  result.rtc_status = rtc_status_;
  result.network_state = network_state_;
  result.last_error = last_error_;
  result.network_synchronized = network_synchronized_;
  result.sync_due = sync_due_;
  result.has_last_success = has_last_success_;
  result.consecutive_failures = consecutive_failures_;
  result.has_last_rtc_drift = has_last_rtc_drift_;
  result.last_rtc_drift_seconds = last_rtc_drift_seconds_;
  result.revision = revision_;
  if (has_last_success_) {
    result.last_success_elapsed_ms = elapsed(now_ms, last_success_ms_);
  }
  if (retry_scheduled_) {
    const std::uint32_t waited = elapsed(now_ms, retry_started_ms_);
    const std::uint32_t delay = retry_delay();
    result.next_attempt_in_ms = waited >= delay ? 0U : delay - waited;
  } else if (has_last_success_ && !sync_due_) {
    const std::uint32_t since_success = elapsed(now_ms, last_success_ms_);
    result.next_attempt_in_ms =
        since_success >= policy_.regular_sync_interval_ms
            ? 0U
            : policy_.regular_sync_interval_ms - since_success;
  }
  return result;
}

void TimeSyncController::publish(const DateTimeValue& value,
                                 TimeSource source,
                                 std::uint32_t now_ms) noexcept {
  source_ = source;
  ++revision_;
  if (ports_.update.apply != nullptr) {
    ports_.update.apply(ports_.update.context, value, source, now_ms);
  }
}

void TimeSyncController::request_network_sync(std::uint32_t now_ms) noexcept {
  retry_scheduled_ = false;
  attempt_started_ms_ = now_ms;
  if (!ports_.network.request_sync(ports_.network.context)) {
    mark_network_failure(now_ms, TimeSyncError::network_start);
    return;
  }
  network_state_ = NetworkState::syncing;
  last_error_ = TimeSyncError::none;
}

void TimeSyncController::mark_network_failure(
    std::uint32_t now_ms, TimeSyncError error) noexcept {
  if (consecutive_failures_ < kMaximumFailureCount) {
    ++consecutive_failures_;
  }
  retry_scheduled_ = true;
  retry_started_ms_ = now_ms;
  network_state_ = NetworkState::retry_waiting;
  last_error_ = error;
}

bool TimeSyncController::consume_network_sync(
    std::uint32_t now_ms) noexcept {
  const std::uint32_t generation =
      ports_.network.sync_generation(ports_.network.context);
  if (generation == observed_generation_) {
    return false;
  }
  observed_generation_ = generation;

  DateTimeValue network_value{};
  if (!ports_.system.read_local(ports_.system.context, network_value) ||
      !is_valid(network_value)) {
    mark_network_failure(now_ms, TimeSyncError::system_read);
    return true;
  }

  publish(network_value, TimeSource::network, now_ms);
  network_synchronized_ = true;
  sync_due_ = false;
  has_last_success_ = true;
  retry_scheduled_ = false;
  consecutive_failures_ = 0U;
  last_success_ms_ = now_ms;
  network_state_ = NetworkState::synced;
  last_error_ = TimeSyncError::none;
  adjust_rtc_from_network(network_value);
  return true;
}

void TimeSyncController::adjust_rtc_from_network(
    const DateTimeValue& value) noexcept {
  has_last_rtc_drift_ = false;
  last_rtc_drift_seconds_ = 0U;
  if (!policy_.update_rtc_from_network) {
    return;
  }
  if (ports_.rtc.write == nullptr) {
    rtc_status_ = RtcStatus::not_detected;
    last_error_ = TimeSyncError::rtc_write;
    return;
  }

  bool should_write = true;
  DateTimeValue rtc_value{};
  if (ports_.rtc.read != nullptr) {
    rtc_status_ = ports_.rtc.read(ports_.rtc.context, rtc_value);
    if (rtc_status_ == RtcStatus::ready && is_valid(rtc_value)) {
      last_rtc_drift_seconds_ = difference_seconds(rtc_value, value);
      has_last_rtc_drift_ = true;
      should_write = last_rtc_drift_seconds_ >
                     policy_.rtc_write_threshold_seconds;
    } else if (rtc_status_ == RtcStatus::ready) {
      rtc_status_ = RtcStatus::invalid;
    }
  }
  if (!should_write) {
    return;
  }

  const RtcStatus write_status =
      ports_.rtc.write(ports_.rtc.context, value);
  if (write_status == RtcStatus::ready) {
    rtc_status_ = RtcStatus::ready;
    return;
  }
  rtc_status_ = write_status == RtcStatus::not_detected
                    ? RtcStatus::not_detected
                    : RtcStatus::write_error;
  last_error_ = TimeSyncError::rtc_write;
}

std::uint32_t TimeSyncController::retry_delay() const noexcept {
  if (consecutive_failures_ <= 1U) {
    return policy_.first_retry_ms;
  }
  if (consecutive_failures_ == 2U) {
    return policy_.second_retry_ms;
  }
  if (consecutive_failures_ == 3U) {
    return policy_.third_retry_ms;
  }
  return policy_.maximum_retry_ms;
}

namespace {

void apply_to_model(void* context, const DateTimeValue& value,
                    TimeSource source, std::uint32_t now_ms) {
  auto* model = static_cast<DateTimeModel*>(context);
  if (model != nullptr) {
    static_cast<void>(model->apply(value, source, now_ms));
  }
}

}  // namespace

TimeUpdateSink date_time_model_sink(DateTimeModel& model) noexcept {
  return {&model, apply_to_model};
}

}  // namespace hmi::time

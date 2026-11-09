#include "hmi/text/input_model.hpp"

#include "hmi/text/utf8.hpp"

namespace hmi::text {

EditStatus InputModel::set_text(std::string_view text) noexcept {
  const auto status = editor_.set_text(text);
  if (status == EditStatus::ok) {
    composer_.cancel();
  }
  return status;
}

EditStatus InputModel::press_key(std::size_t key_index) noexcept {
  const auto layout = keyboard_layout(mode_, shift_);
  if (layout.keys == nullptr || key_index >= layout.size) {
    return EditStatus::invalid_codepoint;
  }
  const auto codepoint = layout.keys[key_index].codepoint;
  if (mode_ == KeyboardMode::hangul) {
    return composer_.input_jamo(editor_, codepoint);
  }
  const auto commit_status = composer_.commit(editor_);
  if (commit_status != EditStatus::ok) {
    return commit_status;
  }
  return editor_.insert_codepoint(codepoint);
}

EditStatus InputModel::insert_codepoint(std::uint32_t codepoint) noexcept {
  const auto commit_status = composer_.commit(editor_);
  if (commit_status != EditStatus::ok) {
    return commit_status;
  }
  return editor_.insert_codepoint(codepoint);
}

EditStatus InputModel::set_mode(KeyboardMode mode) noexcept {
  if (mode == mode_) {
    return EditStatus::ok;
  }
  const auto status = composer_.commit(editor_);
  if (status != EditStatus::ok) {
    return status;
  }
  mode_ = mode;
  shift_ = false;
  return EditStatus::ok;
}

EditStatus InputModel::move_cursor(int codepoint_delta) noexcept {
  const auto status = composer_.commit(editor_);
  if (status != EditStatus::ok) {
    return status;
  }
  return editor_.move_cursor(codepoint_delta);
}

EditStatus InputModel::move_cursor_to(std::size_t display_codepoint_index) noexcept {
  const auto display_count =
      editor_.codepoint_count() + (composer_.is_composing() ? 1U : 0U);
  if (display_codepoint_index > display_count) {
    return EditStatus::cursor_out_of_range;
  }

  const auto status = composer_.commit(editor_);
  if (status != EditStatus::ok) {
    return status;
  }
  return editor_.move_cursor_to(display_codepoint_index);
}

EditStatus InputModel::insert_space() noexcept {
  if (composer_.is_composing()) {
    const auto preview = utf8::encode(composer_.preview_codepoint());
    if (preview.size == 0U) {
      return EditStatus::invalid_codepoint;
    }
    if (editor_.byte_size() + preview.size + 1U > Editor::kMaxBytes) {
      return EditStatus::byte_limit;
    }
    if (editor_.codepoint_count() + 2U > Editor::kMaxCodepoints) {
      return EditStatus::codepoint_limit;
    }
  }

  const auto status = composer_.commit(editor_);
  if (status != EditStatus::ok) {
    return status;
  }
  return editor_.insert_codepoint(' ');
}

EditStatus InputModel::backspace() noexcept {
  return composer_.backspace(editor_);
}

EditStatus InputModel::delete_forward() noexcept {
  const auto status = composer_.commit(editor_);
  if (status != EditStatus::ok) {
    return status;
  }
  return editor_.delete_forward();
}

EditStatus InputModel::complete() noexcept {
  return composer_.commit(editor_);
}

void InputModel::clear() noexcept {
  composer_.cancel();
  editor_.clear();
}

}  // namespace hmi::text

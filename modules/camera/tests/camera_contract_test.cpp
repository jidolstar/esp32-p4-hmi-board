#include <array>
#include <cassert>
#include <limits>
#include <type_traits>
#include <utility>

#include "hmi/camera/camera_frame_lease.hpp"
#include "hmi/camera/gallery_index.hpp"

namespace {

using namespace hmi::camera;

struct Returns {
  std::array<unsigned, 3U> calls{};

  static void record(void* context, std::uint32_t id) noexcept {
    auto& self = *static_cast<Returns*>(context);
    assert(id < self.calls.size());
    ++self.calls[id];
  }
};

void check_frame_return() {
  static_assert(!std::is_copy_constructible_v<CameraFrameLease>);
  static_assert(!std::is_copy_assignable_v<CameraFrameLease>);
  Returns returns;
  const std::array<std::uint8_t, 2U> pixels{0U, 0U};
  {
    CameraFrameLease first({pixels.data(), pixels.size(), 1U, 1U, 1U, 2U},
                           &returns, Returns::record);
    CameraFrameLease moved(std::move(first));
    assert(!first && first.view().pixels == nullptr);
    assert(moved && moved.view().pixels == pixels.data());
    assert(returns.calls[1] == 0U);

    CameraFrameLease second({pixels.data(), pixels.size(), 2U, 1U, 1U, 2U},
                            &returns, Returns::record);
    second = std::move(moved);
    assert(returns.calls[2] == 1U && returns.calls[1] == 0U);
    assert(!moved && second.view().frame_id == 1U);
    second.reset();
    second.reset();
    assert(!second && second.view().pixels == nullptr);
    assert(returns.calls[1] == 1U);

    CameraFrameLease scoped({pixels.data(), pixels.size(), 0U, 1U, 1U, 2U},
                            &returns, Returns::record);
    assert(scoped);
  }
  assert((returns.calls == std::array<unsigned, 3U>{1U, 1U, 1U}));
}

void check_canonical_names() {
  constexpr auto max_id = std::numeric_limits<PhotoId>::max();
  const auto max_name = photo_filename(max_id);
  assert(std::string_view(max_name.data()) == "IMG_18446744073709551615.jpg");
  assert(parse_photo_filename(max_name.data()) == max_id);
  assert(photo_filename(0U).front() == '\0');

  for (const auto invalid : {"IMG_0.jpg", "IMG_01.jpg", "IMG_+1.jpg", "IMG_.jpg",
                             "IMG_18446744073709551616.jpg", "IMG_1.tmp", "IMG_1.jpg.tmp",
                             "../IMG_1.jpg", "sub/IMG_1.jpg", "sub\\IMG_1.jpg", "IMG_1.JPG"}) {
    assert(parse_photo_filename(invalid) == 0U);
  }
  constexpr char embedded_null[] = "IMG_1\0.jpg";
  assert(parse_photo_filename({embedded_null, sizeof(embedded_null) - 1U}) == 0U);
}

void check_bounded_index() {
  GalleryIndex index;
  GalleryPage page;
  index.read_page(0U, page);
  assert(page.count == 0U);
  assert(!index.add("IMG_100.tmp", {}));
  assert(!index.add("../IMG_100.jpg", {}));

  for (PhotoId id = 61U; id > 1U; --id) {
    const auto name = photo_filename(id);
    assert(index.add(name.data(), {0U, 0U, 42U, 1288U, 728U}));
  }
  assert(index.size() == 60U);
  assert(!index.add("IMG_1.jpg", {}));
  assert(!index.add("IMG_61.jpg", {}));
  assert(!index.add("IMG_33.jpg", {}));
  assert(index.add("IMG_62.jpg", {0U, 0U, 90U, 1288U, 728U}));
  assert(index.contains(62U) && index.contains(3U) && !index.contains(2U));

  index.read_page(0U, page);
  assert(page.count == 8U && page.items[0].id == 62U && page.items[7].id == 55U);
  assert(page.items[0].bytes == 90U && page.items[1].bytes == 42U);
  index.read_page(1U, page);
  assert(page.count == 8U && page.items[0].id == 54U);
  index.read_page(7U, page);
  assert(page.count == 4U && page.items[0].id == 6U && page.items[3].id == 3U);
  assert(page.items[4].id == 0U);
  index.read_page(std::numeric_limits<std::size_t>::max(), page);
  assert(page.count == 0U && page.items[0].id == 0U);

  index.clear();
  assert(index.size() == 0U && !index.contains(62U));
  assert(index.add("IMG_8.jpg", {}) && index.add("IMG_2.jpg", {}) && index.add("IMG_5.jpg", {}));
  index.read_page(0U, page);
  assert(page.count == 3U && page.items[0].id == 8U && page.items[1].id == 5U && page.items[2].id == 2U);
}

}  // namespace

int main() {
  check_frame_return();
  check_canonical_names();
  check_bounded_index();
}

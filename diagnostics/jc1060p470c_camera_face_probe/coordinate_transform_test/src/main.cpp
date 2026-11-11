#include <cassert>
#include <iostream>

#include "face_coordinate_transform.hpp"

namespace {

using face_probe::Point;
using face_probe::Rect;
using face_probe::Rotation;

void expect_point(Point actual, Point expected)
{
    assert(actual.x == expected.x);
    assert(actual.y == expected.y);
}
} // namespace

int main()
{
    const Rect preview = {0, 0, 672, 504};
    const Rect detector_content = face_probe::fit_contain(160, 120, preview);
    assert(detector_content.x == 0);
    assert(detector_content.y == 0);
    assert(detector_content.width == 672);
    assert(detector_content.height == 504);

    expect_point(face_probe::map_to_content({0, 0}, 160, 120, detector_content,
                                            Rotation::deg_0, false),
                 {0, 0});
    expect_point(face_probe::map_to_content({159, 119}, 160, 120, detector_content,
                                            Rotation::deg_0, false),
                 {671, 503});
    expect_point(face_probe::map_to_content({0, 0}, 160, 120, detector_content,
                                            Rotation::deg_0, true),
                 {671, 0});

    const Rect rotated_content = face_probe::fit_contain(120, 160, preview);
    expect_point(face_probe::map_to_content({0, 0}, 160, 120, rotated_content,
                                            Rotation::deg_90, false),
                 {rotated_content.x + rotated_content.width - 1, rotated_content.y});
    expect_point(face_probe::map_to_content({159, 119}, 160, 120, rotated_content,
                                            Rotation::deg_270, false),
                 {rotated_content.x + rotated_content.width - 1, rotated_content.y});

    const Rect camera_content = face_probe::fit_contain(1288, 728, preview);
    assert(camera_content.x == 0);
    assert(camera_content.y == 62);
    assert(camera_content.width == 672);
    assert(camera_content.height == 380);
    expect_point(face_probe::map_to_content({-10, 900}, 1288, 728, camera_content,
                                            Rotation::deg_0, false),
                 {0, 441});

    std::cout << "C0_5_COORDINATE_TRANSFORM_PASS\n";
    return 0;
}

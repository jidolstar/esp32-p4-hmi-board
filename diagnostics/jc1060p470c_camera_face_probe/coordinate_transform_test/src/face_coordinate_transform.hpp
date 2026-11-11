#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace face_probe {

struct Point {
    int x;
    int y;
};

struct Rect {
    int x;
    int y;
    int width;
    int height;
};

enum class Rotation : uint8_t {
    deg_0,
    deg_90,
    deg_180,
    deg_270,
};

/**
 * source image를 crop하지 않고 viewport 안에 맞춘 실제 image content rect를 계산한다.
 *
 * 반환 rect는 letterbox 영역을 제외한다. detector 좌표는 이 rect에만 매핑해야 하며
 * viewport 전체에 바로 늘리면 안 된다.
 */
inline Rect fit_contain(int source_width, int source_height, const Rect &viewport)
{
    if (source_width <= 0 || source_height <= 0 || viewport.width <= 0 || viewport.height <= 0) {
        return {viewport.x, viewport.y, 0, 0};
    }

    int content_width = viewport.width;
    int content_height = static_cast<int>(std::lround(
        static_cast<double>(viewport.width) * source_height / source_width));
    if (content_height > viewport.height) {
        content_height = viewport.height;
        content_width = static_cast<int>(std::lround(
            static_cast<double>(viewport.height) * source_width / source_height));
    }
    return {
        viewport.x + (viewport.width - content_width) / 2,
        viewport.y + (viewport.height - content_height) / 2,
        content_width,
        content_height,
    };
}

/**
 * detector 좌표를 회전·mirror 뒤 letterbox content rect 좌표로 변환한다.
 *
 * source 경계 밖 결과는 가장 가까운 image edge로 clamp한다. 원본 frame이나 detector
 * vector의 ownership은 갖지 않으며 고정 크기 좌표만 반환한다.
 */
inline Point map_to_content(Point source,
                            int source_width,
                            int source_height,
                            const Rect &content,
                            Rotation rotation,
                            bool mirror_horizontal)
{
    if (source_width <= 0 || source_height <= 0 || content.width <= 0 || content.height <= 0) {
        return {content.x, content.y};
    }

    source.x = std::clamp(source.x, 0, source_width - 1);
    source.y = std::clamp(source.y, 0, source_height - 1);

    int rotated_x = source.x;
    int rotated_y = source.y;
    int rotated_width = source_width;
    int rotated_height = source_height;
    switch (rotation) {
    case Rotation::deg_0:
        break;
    case Rotation::deg_90:
        rotated_x = source_height - 1 - source.y;
        rotated_y = source.x;
        rotated_width = source_height;
        rotated_height = source_width;
        break;
    case Rotation::deg_180:
        rotated_x = source_width - 1 - source.x;
        rotated_y = source_height - 1 - source.y;
        break;
    case Rotation::deg_270:
        rotated_x = source.y;
        rotated_y = source_width - 1 - source.x;
        rotated_width = source_height;
        rotated_height = source_width;
        break;
    }

    if (mirror_horizontal) {
        rotated_x = rotated_width - 1 - rotated_x;
    }

    const auto scale_axis = [](int value, int source_extent, int target_origin, int target_extent) {
        if (source_extent <= 1 || target_extent <= 1) {
            return target_origin;
        }
        return target_origin + static_cast<int>(std::lround(
            static_cast<double>(value) * (target_extent - 1) / (source_extent - 1)));
    };
    return {
        scale_axis(rotated_x, rotated_width, content.x, content.width),
        scale_axis(rotated_y, rotated_height, content.y, content.height),
    };
}

} // namespace face_probe

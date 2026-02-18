#pragma once

#include <cstdint>
#include <opencv2/core/types.hpp>

// Undefine Windows RGB macro if it exists to prevent conflicts
#ifdef RGB
#undef RGB
#endif

// Basic types for performance
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;

// Color structures
// Note: Windows defines RGB as a macro, so we use RGBColor instead
struct RGBColor {
    u8 r, g, b;
    RGBColor() : r(0), g(0), b(0) {}
    RGBColor(u8 r, u8 g, u8 b) : r(r), g(g), b(b) {}

    u32 toU32() const {
        return (r << 16) | (g << 8) | b;
    }

    static RGBColor fromU32(u32 pixel) {
        return {
            static_cast<u8>((pixel >> 16) & 0xFF),
            static_cast<u8>((pixel >> 8) & 0xFF),
            static_cast<u8>(pixel & 0xFF)
        };
    }
};

struct HSV {
    f32 h, s, v;
    HSV() : h(0), s(0), v(0) {}
    HSV(f32 h, f32 s, f32 v) : h(h), s(s), v(v) {}
};

// Screen and FOV structures
struct ScreenInfo {
    i32 width;
    i32 height;
    i32 centerX;
    i32 centerY;

    ScreenInfo() : width(0), height(0), centerX(0), centerY(0) {}
    ScreenInfo(i32 w, i32 h) : width(w), height(h), centerX(w / 2), centerY(h / 2) {}
};

struct FOV {
    i32 x;
    i32 y;
    i32 width;
    i32 height;

    FOV() : x(0), y(0), width(0), height(0) {}
    FOV(i32 x, i32 y, i32 w, i32 h) : x(x), y(y), width(w), height(h) {}

    cv::Rect toCvRect() const {
        return cv::Rect(x, y, width, height);
    }
};

// Target detection results
struct Target {
    cv::Rect bounds;
    cv::Point center;
    i32 height;
    f32 confidence;

    Target() : height(0), confidence(0.0f) {}
    Target(const cv::Rect& rect, f32 conf = 1.0f)
        : bounds(rect),
        center(rect.x + rect.width / 2, rect.y + rect.height / 2),
        height(rect.height),
        confidence(conf) {
    }

    bool isValid() const {
        return bounds.area() > 0 && confidence > 0.1f;
    }
};

// Aiming vectors
struct AimVector {
    f32 x;
    f32 y;
    f32 distance;

    AimVector() : x(0), y(0), distance(0) {}
    AimVector(f32 x, f32 y) : x(x), y(y), distance(sqrtf(x* x + y * y)) {}

    AimVector normalized() const {
        if (distance < 0.001f) return AimVector(0, 0);
        return AimVector(x / distance, y / distance);
    }
};

// Performance metrics
struct PerformanceStats {
    f32 fps;
    f32 frameTimeMs;
    f32 detectionTimeMs;
    f32 aimTimeMs;
    i32 targetsFound;

    PerformanceStats() : fps(0), frameTimeMs(0), detectionTimeMs(0), aimTimeMs(0), targetsFound(0) {}
};

// Constants
namespace Constants {
    constexpr f32 PI = 3.14159265358979323846f;
    constexpr f32 DEG_TO_RAD = PI / 180.0f;
    constexpr f32 RAD_TO_DEG = 180.0f / PI;

    // Default game sensitivities (adjust based on game)
    constexpr f32 CSGO_SENS_MULTIPLIER = 0.022f;
    constexpr f32 VALORANT_SENS_MULTIPLIER = 0.07f;
    constexpr f32 AIMLABS_SENS_MULTIPLIER = 0.022f;

    // Mouse movement constraints
    constexpr i32 MAX_MOUSE_MOVE = 50;
    constexpr i32 MIN_MOUSE_MOVE = -50;

    // Timing
    constexpr u64 MS_PER_SECOND = 1000;
    constexpr u64 US_PER_SECOND = 1000000;
    constexpr u64 NS_PER_SECOND = 1000000000;
}

// Helper macros
#define SAFE_DELETE(ptr) { if (ptr) { delete ptr; ptr = nullptr; } }
#define SAFE_DELETE_ARRAY(ptr) { if (ptr) { delete[] ptr; ptr = nullptr; } }
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
#define LERP(a, b, t) ((a) + (t) * ((b) - (a)))

// Platform-specific
#ifdef _WIN32
#define FORCEINLINE __forceinline
#else
#define FORCEINLINE inline __attribute__((always_inline))
#endif

// Memory alignment for performance
#ifdef _WIN32
#define ALIGNAS(x) __declspec(align(x))
#else
#define ALIGNAS(x) alignas(x)
#endif


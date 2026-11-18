#pragma once
/// @file nnh/math.hpp
/// Small value types for 2D: points and sizes (Vec2), rectangles in pixels (Rect) and regions inside a
/// texture (Region). 3D types (vectors, matrices) will live beside these, never replace them.
/// See docs/api/math.md.

namespace nnh {

/// A 2D point, size or direction in pixels (float, so movement can be smooth).
struct Vec2 {
    float x = 0;
    float y = 0;

    constexpr Vec2& operator+=(Vec2 o) {
        x += o.x;
        y += o.y;
        return *this;
    }
    constexpr Vec2& operator-=(Vec2 o) {
        x -= o.x;
        y -= o.y;
        return *this;
    }
    constexpr Vec2& operator*=(float s) {
        x *= s;
        y *= s;
        return *this;
    }
    friend constexpr Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
    friend constexpr Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
    friend constexpr Vec2 operator-(Vec2 a) { return {-a.x, -a.y}; }
    friend constexpr Vec2 operator*(Vec2 a, float s) { return {a.x * s, a.y * s}; }
    friend constexpr Vec2 operator*(float s, Vec2 a) { return {a.x * s, a.y * s}; }
    friend constexpr Vec2 operator/(Vec2 a, float s) { return {a.x / s, a.y / s}; }
    friend constexpr bool operator==(Vec2 a, Vec2 b) { return a.x == b.x && a.y == b.y; }
};

/// A rectangle on a screen or in the world: top-left corner and size, in pixels.
struct Rect {
    float x = 0;
    float y = 0;
    float w = 0;
    float h = 0;
};

/// A rectangle of whole pixels inside a texture (e.g. one frame of a sprite sheet). An empty region
/// (no width or height) means "the whole texture".
struct Region {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    constexpr bool empty() const { return w <= 0 || h <= 0; }
};

} // namespace nnh

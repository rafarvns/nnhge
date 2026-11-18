# math — 2D value types

Header: `<nnh/math.hpp>`

Plain structs for 2D positions and rectangles, used by the drawing API ([gfx.md](gfx.md)). 3D types
(vectors, matrices) will be added beside them later; these keep their names.

| Type | Fields | Use |
|------|--------|-----|
| `nnh::Vec2` | `float x, y` | a point, size or direction in pixels |
| `nnh::Rect` | `float x, y, w, h` | a rectangle: top-left corner and size |
| `nnh::Region` | `int x, y, w, h` | whole pixels inside a texture (a sprite-sheet frame); `empty()` = no width or height, meaning "the whole texture" |

`Vec2` supports `+`, `-` (also unary), `* float`, `float *`, `/ float`, `+=`, `-=`, `*=`, `==`, `!=`, all
`constexpr`:

```cpp
nnh::Vec2 position{40, 100};
const nnh::Vec2 velocity{1.5f, 0};
position += velocity * speed;
```

Positions are `float` so movement can be smooth; drawing rounds the final screen position to whole
pixels ([rendering.md](../architecture/rendering.md)).

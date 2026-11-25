#pragma once
// The check's fixed probe scenes (spec 026 AC-7, FR-24): each draws a known picture with an orthographic camera
// (400×240 shows 4 × 2.4 world units, 100 px per unit) and lists probe pixels with the colour FR-10's formula gives
// and a tolerance per channel (plan P23: ±2 flat and nearest-textured, ±3 blended, ±10 lit). Probes sit ≥ 2 px from
// any edge.

#include <nnh/gfx3d.hpp>
#include <nnh/mesh.hpp>

#include "world.hpp"

struct ProbeExpect {
    nnh::Probe at;
    nnh::Color want;
    int tolerance = 2;
};

class ProbeScenes {
public:
    static constexpr int kScenes = 6;
    static constexpr int kMaxProbes = 4;

    /// Makes the scenes' own meshes (a textured quad, a fine sphere, plain quads). @return false on failure.
    bool load(const World& world);
    /// Draws scene `i` (3D on the top screen, plus its 2D) for this frame.
    void draw(int i) const;
    /// Scene `i`'s name and probes; @return the probe count.
    int expect(int i, ProbeExpect* out) const;
    static const char* name(int i);

    /// The stereo scene (probe_stereo.cpp, spec 026 AC-7): a red wall on the focal plane, a narrow green card nearer;
    /// the eyes are a fixed 0.25 units apart whatever the slider (> 0), so the card shifts ±7.8 px between them.
    void draw_stereo(float slider) const;
    /// Its probes in `eye` (0 left, 1 right); @return the count.
    static int expect_stereo(int eye, ProbeExpect* out);

private:
    nnh::Mesh quad_tex_, quad_, sphere_, cube_;
    const World* world_ = nullptr;
};

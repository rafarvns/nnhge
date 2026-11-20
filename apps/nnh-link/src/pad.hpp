#pragma once
// The number pad on the bottom screen (pairing code, server address): where each key is, which key a touch
// hits, and a focus the D-Pad moves (A presses it). Pure; unit tested in tests/test_pad.cpp.
//
//   [1][2][3][<]
//   [4][5][6][.]      '<' erases, '.' only for addresses,
//   [7][8][9][OK]     OK (kOk) takes two rows
//      [0]   [OK]

namespace link_app {

class Pad {
public:
    static constexpr char kErase = '<';
    static constexpr char kOk = '\n';
    static constexpr int kColumns = 4;
    static constexpr int kRows = 4;
    static constexpr int kLeft = 40; ///< bottom-screen pixels
    static constexpr int kTop = 72;
    static constexpr int kKeyWidth = 60;
    static constexpr int kKeyHeight = 40;

    struct Key {
        char c;
        int column, row, rows;
    };
    static constexpr int kKeyCount = 13;

    /// `with_dot`: the '.' key is there (addresses), else it is skipped and can't be hit.
    explicit Pad(bool with_dot) : with_dot_(with_dot) {}

    static const Key& key(int i);
    bool enabled(int i) const { return with_dot_ || key(i).c != '.'; }
    /// The key at a bottom-screen point, or 0.
    char hit(int x, int y) const;
    /// Moves the focus one key in a direction (dx, dy in -1..1), skipping gaps; stays at the edge.
    void move(int dx, int dy);
    int focus() const { return focus_; }
    char focused() const { return key(focus_).c; }

private:
    int index_at(int column, int row) const; ///< -1 for a gap or a disabled key
    bool with_dot_;
    int focus_ = 0;
};

} // namespace link_app

#pragma once
// AC-6's workloads (spec 028), pure: each call is one frame of work that bench_scene.cpp times. idle128 (a 128-widget UI
// on both roots, no input), move128 (fresh up/down presses in a 20-button VBox), geo128 (left/right presses resolved by
// the geometric rule between two columns), relayout128 (text_changed() with 64 keyed texts), list10000 / list20 (one
// row a frame, 8 down then 8 up, around item 5000 of 10 000 or in a 20-item list; the update is timed, the draw that
// fills the row coming into view is not).

#include <nnh/ui.hpp>

class UiBench {
public:
    bool init(); ///< false when out of memory

    void idle128();
    void move128(int frame);
    void geo128(int frame);
    void relayout128();
    void list10000(int frame);
    void list20(int frame);
    void draw_list(bool long_list) const; ///< untimed: draws (and fills the rows coming into view)
    int list_fills() const;

private:
    nnh::Ui big_;   ///< 128 widgets
    nnh::Ui list_a_, list_b_;
    nnh::UiWidget column_[2]{}, long_{}, short_{};
};

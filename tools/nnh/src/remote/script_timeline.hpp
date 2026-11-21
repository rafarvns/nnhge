#pragma once
// Script steps → frame-stamped remote messages (spec 013, plan "Script language" and "Runner"). The script
// clock starts at frame 1, but after an `expect` it continues from the frame after the matching line, which
// only the run knows: so the timeline is cut into segments at each `expect`, and frames are offsets from the
// segment's first frame (the runner adds the base). Pure; tested in tools/nnh/tests/test_script_timeline.cpp.

#include <cstdint>
#include <string>
#include <vector>

#include "remote/remote_wire.hpp"
#include "remote/script.hpp"

namespace nnh::tool::remote {

namespace wire = nnh::detail::remote;

struct TimedEvent {
    enum class Kind : std::uint8_t { Send, FailOn };
    Kind kind = Kind::Send;
    std::uint32_t offset = 0; ///< frames after the segment's first frame
    int line = 0;             ///< the script line that made it
    wire::Message message;    ///< Send: type and arguments (frame, seq and Screenshot id are the runner's)
    std::string text;         ///< FailOn: the pattern; Screenshot: NAME
};

struct Segment {
    std::vector<TimedEvent> events; ///< in step order; offsets never go down
    std::uint32_t length = 0;       ///< the clock at the segment's end (offset of the next step)
    Step until;                     ///< the Expect or End that closes it
};

/// Builds segments step by step. Remembers where the touch is, so `drag` starts from it across segments
/// (and across lines typed in interactive mode).
class Timeline {
public:
    /// Adds a step at the clock: input → messages, `wait`/`hold`/`tap`/`drag` move the clock, `fail-on` →
    /// a FailOn event, `expect`/`end` close the segment.
    ///   press/release B → Buttons; release all → Buttons(release every button), both sticks centred, touch up;
    ///   hold B N → press now, release at +N (clock +N); tap B → press now, release at +1 (clock +2);
    ///   drag X Y N → touch moves at +1 … +N, the last step at X Y (clock +N+1, so X Y is held for a frame
    ///   before what follows); screenshot → both screens at the end of this frame (clock +1, so what follows
    ///   isn't in the picture).
    void add(const Step& step);

    /// Segments closed by an Expect or End, in order.
    const std::vector<Segment>& segments() const { return closed_; }

    /// The events and length added since the last closed segment, which start over (interactive mode).
    Segment take_open();

private:
    void send(const wire::Message& message, std::uint32_t offset, int line, std::string text = {});

    std::vector<Segment> closed_;
    Segment open_;
    std::int16_t touch_x_ = 0;
    std::int16_t touch_y_ = 0;
};

/// The whole script as segments (the last one closed by its End).
std::vector<Segment> build_timeline(const Script& script);

} // namespace nnh::tool::remote

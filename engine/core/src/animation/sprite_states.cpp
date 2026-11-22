// The state machine's data (spec 016 FR-12, FR-13; T5.1): states, next-on-finish, transition rules and the rule
// resolution (most specific first).

#include "animation/sprite_library_internal.hpp"

namespace nnh {

using namespace detail::spritefmt;

namespace detail {

TransitionEntry sprite_resolve_rule(const TransitionEntry* rules, int count, std::uint8_t from, std::uint8_t to) {
    const std::uint8_t shapes[4][2] = {{from, to}, {from, kAnyState}, {kAnyState, to}, {kAnyState, kAnyState}};
    for (const auto& shape : shapes) {
        for (int i = 0; i < count; ++i) {
            if (rules[i].from == shape[0] && rules[i].to == shape[1]) return rules[i];
        }
    }
    return {from, to, kNow, 0};
}

} // namespace detail

Result<SpriteStateId> SpriteLibrary::add_state(const char* name, SpriteClipId clip) {
    if (block_ == nullptr) return Error::make(ErrorCode::InvalidArgument, "add_state: empty library");
    const detail::SpriteView v = detail::sprite_view(block_);
    if (const char* why = detail::sprite_name_problem(name)) {
        return Error::make(ErrorCode::InvalidArgument, "add_state: %s", why);
    }
    const std::uint32_t hash = detail::fnv1a32(name);
    for (int i = 0; i < v.count(kStates); ++i) {
        if (v.states()[i].hash == hash) {
            return Error::make(ErrorCode::InvalidArgument, "state \"%s\": a state has this name already", name);
        }
    }
    if (clip.value >= v.count(kClips)) return Error::make(ErrorCode::InvalidArgument, "state \"%s\": unknown clip", name);
    const int index = v.count(kStates);
    if (index >= v.cap(kStates)) {
        return Error::make(ErrorCode::LimitReached, "state \"%s\": the library holds %d states", name, v.cap(kStates));
    }
    StateEntry& s = v.states()[index];
    s = {};
    detail::sprite_store_name(s.name, name);
    s.hash = hash;
    s.clip = clip.value;
    s.next = kNoState;
    v.header().count[kStates] = static_cast<std::uint16_t>(index + 1);
    return SpriteStateId{static_cast<std::uint8_t>(index)};
}

Result<void> SpriteLibrary::set_next_on_finish(SpriteStateId state, SpriteStateId next) {
    if (block_ == nullptr) return Error::make(ErrorCode::InvalidArgument, "set_next_on_finish: empty library");
    const detail::SpriteView v = detail::sprite_view(block_);
    if (state.value >= v.count(kStates)) return Error::make(ErrorCode::InvalidArgument, "set_next_on_finish: unknown state");
    if (next.value != kNoState && next.value >= v.count(kStates)) {
        return Error::make(ErrorCode::InvalidArgument, "set_next_on_finish: unknown next state");
    }
    v.states()[state.value].next = next.value;
    return {};
}

Result<void> SpriteLibrary::add_transition(const SpriteTransition& rule) {
    if (block_ == nullptr) return Error::make(ErrorCode::InvalidArgument, "add_transition: empty library");
    const detail::SpriteView v = detail::sprite_view(block_);
    auto known = [&](SpriteStateId s) { return s.value == kAnyState || s.value < v.count(kStates); };
    if (!known(rule.from) || !known(rule.to)) return Error::make(ErrorCode::InvalidArgument, "add_transition: unknown state");
    if (static_cast<int>(rule.when) > kNever) return Error::make(ErrorCode::InvalidArgument, "add_transition: unknown kind");
    for (int i = 0; i < v.count(kTransitions); ++i) {
        const TransitionEntry& t = v.transitions()[i];
        if (t.from == rule.from.value && t.to == rule.to.value) {
            return Error::make(ErrorCode::InvalidArgument, "add_transition: a rule for these states exists already");
        }
    }
    const int index = v.count(kTransitions);
    if (index >= v.cap(kTransitions)) {
        return Error::make(ErrorCode::LimitReached, "add_transition: the library holds %d rules", v.cap(kTransitions));
    }
    v.transitions()[index] = {rule.from.value, rule.to.value, static_cast<std::uint8_t>(rule.when),
                              static_cast<std::uint8_t>(rule.keep_time ? 1 : 0)};
    v.header().count[kTransitions] = static_cast<std::uint16_t>(index + 1);
    return {};
}

SpriteStateId SpriteLibrary::find_state(const char* name) const {
    if (block_ == nullptr || name == nullptr) return {};
    const detail::SpriteView v = detail::sprite_view(block_);
    const std::uint32_t hash = detail::fnv1a32(name);
    for (int i = 0; i < v.count(kStates); ++i) {
        if (v.states()[i].hash == hash) return SpriteStateId{static_cast<std::uint8_t>(i)};
    }
    return {};
}

const char* SpriteLibrary::state_name(SpriteStateId state) const {
    if (block_ == nullptr || state.value >= state_count()) return "";
    return detail::sprite_view(block_).states()[state.value].name;
}

} // namespace nnh

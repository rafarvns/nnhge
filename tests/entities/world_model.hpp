#pragma once
// A naive reference model of a world (spec 020 T3.4): plain arrays and linear searches, the plan's rules written the
// obvious way: lowest free slot, quarantine until flush, generations +1 per destroy, hooks in kind order with the
// data intact, destroys from hooks queued FIFO, newcomers of an iteration not visited, tags in fixed words. Compared
// with nnh::World operation by operation (test_world_model.cpp, fuzz_world_ops.cpp).

#include <cstdint>

namespace world_model {

inline constexpr int kKinds = 3;
inline constexpr int kMaxEntities = 256;
inline constexpr int kMaxTags = 8;

struct Ent {
    std::uint32_t gen = 1;
    bool live = false, quar = false, dying = false, fresh = false;
    int slot[kKinds] = {-1, -1, -1};
    std::uint32_t tags[kMaxTags] = {};
};

struct Comp {
    bool live = false, quar = false, fresh = false, removing = false;
    int owner = -1;
    int value = 0;
};

class Model;
/// The kinds' remove hooks: kind k's hook for entity `slot`'s component of value `value`.
using Hook = void (*)(Model& m, int kind, int slot, int value);

class Model {
public:
    Model(int entities, int tags, Hook hook) : n_(entities), tags_(tags), hook_(hook) {}

    // Handles are (slot | generation << 16), 0 = null, as nnh::Entity::bits().
    std::uint32_t handle(int slot) const { return static_cast<std::uint32_t>(slot) | ents_[slot].gen << 16; }
    int live_slot(std::uint32_t h) const {
        const int slot = static_cast<int>(h & 0xFFFF);
        if (h == 0 || slot >= n_ || !ents_[slot].live || ents_[slot].gen != h >> 16) return -1;
        return slot;
    }
    bool alive(std::uint32_t h) const { return live_slot(h) >= 0; }
    int count() const {
        int c = 0;
        for (int i = 0; i < n_; ++i) c += ents_[i].live ? 1 : 0;
        return c;
    }
    const Ent& ent(int slot) const { return ents_[slot]; }
    const Comp& comp(int kind, int slot) const { return comps_[kind][slot]; }
    int kind_extent(int kind) const { return kext_[kind]; }
    int entity_extent() const { return eext_; }
    bool clearing() const { return clearing_; }
    /// Whether `kind` has a slot neither live nor quarantined (the world asserts when it has none).
    bool has_free(int kind) const {
        for (int c = 0; c < n_; ++c)
            if (!comps_[kind][c].live && !comps_[kind][c].quar) return true;
        return false;
    }
    /// Live and quarantined slots of a kind (kind < 0: of the entities).
    int used(int kind) const {
        int n = 0;
        for (int i = 0; i < n_; ++i) {
            if (kind < 0) {
                n += ents_[i].live || ents_[i].quar ? 1 : 0;
            } else {
                n += comps_[kind][i].live || comps_[kind][i].quar ? 1 : 0;
            }
        }
        return n;
    }
    bool has_free_entity() const {
        for (int i = 0; i < n_; ++i)
            if (!ents_[i].live && !ents_[i].quar) return true;
        return false;
    }

    std::uint32_t spawn() {
        if (clearing_) return 0;
        for (int i = 0; i < n_; ++i) {
            Ent& e = ents_[i];
            if (e.live || e.quar) continue;
            e.live = true;
            e.fresh = iterating_ > 0;
            if (i + 1 > eext_) eext_ = i + 1;
            return handle(i);
        }
        return 0; // full (the real world asserts: callers avoid it)
    }

    /// Mirrors add<T>(e, {value}): false when stale, dying, already there or full.
    bool add(int kind, std::uint32_t h, int value) {
        const int s = live_slot(h);
        if (s < 0 || ents_[s].dying || ents_[s].slot[kind] >= 0) return false;
        for (int c = 0; c < n_; ++c) {
            Comp& x = comps_[kind][c];
            if (x.live || x.quar) continue;
            x = {true, false, iterating_ > 0, false, s, value};
            ents_[s].slot[kind] = c;
            if (c + 1 > kext_[kind]) kext_[kind] = c + 1;
            return true;
        }
        return false;
    }

    bool remove(int kind, std::uint32_t h) {
        const int s = live_slot(h);
        if (s < 0 || ents_[s].dying) return false;
        const int c = ents_[s].slot[kind];
        if (c < 0 || comps_[kind][c].removing) return false;
        comps_[kind][c].removing = true;
        ++depth_;
        hook_(*this, kind, s, comps_[kind][c].value);
        --depth_;
        comps_[kind][c].removing = false;
        detach(kind, s);
        if (depth_ == 0) drain();
        return true;
    }

    bool destroy(std::uint32_t h) {
        const int s = live_slot(h);
        if (s < 0 || ents_[s].dying) return false;
        ents_[s].dying = true;
        if (depth_ > 0) {
            queue_[qn_++] = s;
            return true;
        }
        ++depth_;
        process(s);
        --depth_;
        drain();
        return true;
    }

    void flush() {
        for (int i = 0; i < n_; ++i) ents_[i].quar = false;
        for (auto& kind : comps_)
            for (int c = 0; c < n_; ++c) kind[c].quar = false;
    }

    void clear() {
        clearing_ = true;
        for (int i = 0; i < n_; ++i)
            if (ents_[i].live) destroy(handle(i));
        flush();
        eext_ = 0;
        for (int& e : kext_) e = 0;
        clearing_ = false;
    }

    bool add_tag(std::uint32_t h, std::uint32_t t) {
        const int s = live_slot(h);
        if (s < 0 || t == 0) return false;
        for (int i = 0; i < tags_; ++i)
            if (ents_[s].tags[i] == t) return false;
        for (int i = 0; i < tags_; ++i) {
            if (ents_[s].tags[i] != 0) continue;
            ents_[s].tags[i] = t;
            return true;
        }
        return false;
    }
    bool remove_tag(std::uint32_t h, std::uint32_t t) {
        const int s = live_slot(h);
        if (s < 0 || t == 0) return false;
        for (int i = 0; i < tags_; ++i) {
            if (ents_[s].tags[i] != t) continue;
            ents_[s].tags[i] = 0;
            return true;
        }
        return false;
    }
    /// add_tag(h, t) would assert: h live, without t, and no empty tag word.
    bool tag_full(std::uint32_t h, std::uint32_t t) const {
        const int s = live_slot(h);
        if (s < 0 || has_tag(h, t)) return false;
        for (int i = 0; i < tags_; ++i)
            if (ents_[s].tags[i] == 0) return false;
        return true;
    }
    bool has_tag(std::uint32_t h, std::uint32_t t) const {
        const int s = live_slot(h);
        if (s < 0 || t == 0) return false;
        for (int i = 0; i < tags_; ++i)
            if (ents_[s].tags[i] == t) return true;
        return false;
    }
    int find(std::uint32_t t, std::uint32_t* out, int capacity) const {
        int total = 0;
        for (int i = 0; i < n_ && t != 0; ++i) {
            if (!ents_[i].live) continue;
            bool hit = false;
            for (int j = 0; j < tags_; ++j) hit = hit || ents_[i].tags[j] == t;
            if (!hit) continue;
            if (total < capacity) out[total] = handle(i);
            ++total;
        }
        return total;
    }

    /// Visits entities having all `kinds` (count >= 1), in ascending slots of kinds[0]'s storage, with fn(handle).
    template <typename F>
    void each(const int* kinds, int count, F&& fn) {
        ++iterating_;
        const int extent = kext_[kinds[0]];
        for (int c = 0; c < extent; ++c) {
            const Comp& a = comps_[kinds[0]][c];
            if (!a.live || a.fresh) continue;
            bool all = true;
            for (int k = 1; k < count; ++k) {
                const int s = ents_[a.owner].slot[kinds[k]];
                all = all && s >= 0 && !comps_[kinds[k]][s].fresh;
            }
            if (all) fn(handle(a.owner));
        }
        end_iteration();
    }
    /// Visits every live entity in ascending slots having `tag` (0: every one): entities(), each_tagged.
    template <typename F>
    void each_entity(std::uint32_t tag, F&& fn) {
        ++iterating_;
        const int extent = eext_;
        for (int i = 0; i < extent; ++i) {
            if (!ents_[i].live || ents_[i].fresh) continue;
            bool hit = tag == 0;
            for (int j = 0; j < tags_; ++j) hit = hit || ents_[i].tags[j] == tag;
            if (hit) fn(handle(i));
        }
        end_iteration();
    }

private:
    void end_iteration() {
        if (--iterating_ > 0) return;
        for (Ent& e : ents_) e.fresh = false;
        for (auto& kind : comps_)
            for (Comp& c : kind) c.fresh = false;
    }

    void detach(int kind, int s) {
        const int c = ents_[s].slot[kind];
        comps_[kind][c] = {false, true, false, false, -1, 0};
        ents_[s].slot[kind] = -1;
    }

    void process(int s) {
        for (int k = 0; k < kKinds; ++k) {
            const int c = ents_[s].slot[k];
            if (c < 0) continue;
            comps_[k][c].removing = true;
            ++depth_;
            hook_(*this, k, s, comps_[k][c].value);
            --depth_;
            comps_[k][c].removing = false;
        }
        for (int k = 0; k < kKinds; ++k)
            if (ents_[s].slot[k] >= 0) detach(k, s);
        Ent& e = ents_[s];
        for (std::uint32_t& t : e.tags) t = 0;
        e.gen = e.gen == 65535 ? 1 : e.gen + 1;
        e.live = false;
        e.quar = true;
        e.dying = false;
    }

    void drain() {
        for (int i = 0; i < qn_; ++i) {
            ++depth_;
            process(queue_[i]);
            --depth_;
        }
        qn_ = 0;
    }

    int n_;
    int tags_;
    Hook hook_;
    Ent ents_[kMaxEntities];
    Comp comps_[kKinds][kMaxEntities];
    int eext_ = 0;
    int kext_[kKinds] = {};
    int iterating_ = 0;
    int depth_ = 0;
    bool clearing_ = false;
    int queue_[kMaxEntities * 4] = {};
    int qn_ = 0;
};

} // namespace world_model

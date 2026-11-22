// The frame's tile batches (see tile_batches.hpp).

#include "gfx/tile_batches.hpp"

namespace nnh::detail {

void TileBatches::reset(TileBatchRecord* records, int capacity, platform::TileQuad2D* scratch,
                        int scratch_capacity, int max_tiles) {
    records_ = records;
    capacity_ = records != nullptr ? capacity : 0;
    scratch_ = scratch;
    scratch_capacity_ = scratch != nullptr ? scratch_capacity : 0;
    max_tiles_ = max_tiles;
    clear();
}

void TileBatches::clear() {
    count_ = 0;
    tiles_ = 0;
    dropped_ = 0;
}

int TileBatches::add(const TileBatchRecord& record) {
    if (count_ >= capacity_) return -1;
    records_[count_] = record;
    return count_++;
}

const TileBatchRecord* TileBatches::get(int index) const {
    if (index < 0 || index >= count_ || records_[index].expand == nullptr) return nullptr;
    return &records_[index];
}

void TileBatches::forget(const void* source) {
    for (int i = 0; i < count_; ++i) {
        if (records_[i].source == source) records_[i].expand = nullptr;
    }
}

TileBatches::Quads TileBatches::expand(int index, Screen screen, Camera2D camera, int first_row) {
    const TileBatchRecord* r = get(index);
    if (r == nullptr) return {};
    const int left = max_tiles_ - tiles_;
    TileExpandRequest request;
    request.source = r->source;
    request.arg = r->arg;
    request.screen = screen;
    request.camera = r->ignore_camera ? Vec2{} : camera.position;
    request.zoom = r->ignore_camera ? 1.0f : camera.zoom;
    request.first_row = first_row;
    request.chunk = !(left < scratch_capacity_);
    request.offset = r->offset;
    request.clock = r->clock;
    request.out = scratch_;
    request.capacity = left < scratch_capacity_ ? (left > 0 ? left : 0) : scratch_capacity_;
    const TileExpandResult result = r->expand(request);
    tiles_ += result.written;
    dropped_ += result.dropped;
    return {scratch_, result.written, result.next_row, result.dst_w, result.dst_h};
}

} // namespace nnh::detail

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps3recomp (sf3comp); from cps2recomp's port/rewind.h.
//
// States kept in memory by the block (the instant replay's and rewind's: port.h). A CPS-3 state is about 10 MB, its
// 8 MB of character RAM most of it, and from one kept state to the next most of its blocks are the same: so each kept
// state shares the blocks that are the same as the one before it (BlockState), and only the ones that changed are its
// own. Rewind's ring (StateRing, Port::RewindKeep) keeps a state every few frames that way, each with the controls its
// frame ran with; going back is taking the newest out and putting it back.
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <vector>

namespace port {

// A state as blocks, those the same as another's shared with it
struct BlockState {
    static constexpr size_t kBlock = 64 * 1024;
    size_t block = kBlock;
    std::vector<std::shared_ptr<const std::vector<uint8_t>>> blocks;
    size_t size = 0;
    // From a state, in blocks of `blockSize`; the blocks the same as `before`'s shared with it (none: all its own)
    void From(const std::vector<uint8_t> &data, const BlockState *before, size_t blockSize = kBlock) {
        block = blockSize;
        size = data.size();
        const size_t count = (size + block - 1) / block;
        blocks.assign(count, nullptr);
        if (before && before->block != block) {
            before = nullptr;
        }
        for (size_t i = 0; i < count; i++) {
            const size_t at = i * block, length = std::min(block, size - at);
            if (before && i < before->blocks.size() && before->blocks[i]->size() == length &&
                std::memcmp(before->blocks[i]->data(), data.data() + at, length) == 0) {
                blocks[i] = before->blocks[i];
                continue;
            }
            blocks[i] = std::make_shared<const std::vector<uint8_t>>(data.begin() + static_cast<std::ptrdiff_t>(at),
                                                                     data.begin() + static_cast<std::ptrdiff_t>(at + length));
        }
    }
    void Into(std::vector<uint8_t> &data) const {
        data.resize(size);
        for (size_t i = 0; i < blocks.size(); i++) {
            std::memcpy(data.data() + i * block, blocks[i]->data(), blocks[i]->size());
        }
    }
    // The bytes of its blocks that are not `other`'s too (none: all of them)
    size_t Own(const BlockState *other) const {
        size_t bytes = 0;
        for (size_t i = 0; i < blocks.size(); i++) {
            if (!other || i >= other->blocks.size() || other->blocks[i] != blocks[i]) {
                bytes += blocks[i]->size();
            }
        }
        return bytes;
    }
    void Clear() {
        blocks.clear();
        size = 0;
    }
};

// Rewind's states: the newest last, each the state before its frame ran (and the guest stage's board's, which a save
// state does not hold: Machine::KeepStage), with the controls that frame ran with
class StateRing {
public:
    struct Input {
        uint16_t players[4] = {};
        bool service = false, test = false;
    };
    struct Kept {
        uint64_t frame = 0;
        Input input;
        BlockState state, stage;
    };
    // (blocks smaller than the instant replay's: kept every few frames, a state changes in fewer bytes than a block of
    // 64 KB, and the smaller ones share more)
    static constexpr size_t kBlock = 4 * 1024;

    void Clear() {
        kept_.clear();
        bytes_ = 0;
    }
    bool Empty() const { return kept_.empty(); }
    size_t Size() const { return kept_.size(); }
    size_t Bytes() const { return bytes_; } // (the memory of the blocks, each counted once)

    // A state kept, the newest; at most `most` of them and `mostBytes` of memory: the oldest go
    void Push(uint64_t frame, const Input &input, const std::vector<uint8_t> &state, const std::vector<uint8_t> &stage, size_t most,
              size_t mostBytes) {
        const Kept *before = kept_.empty() ? nullptr : &kept_.back();
        Kept kept;
        kept.frame = frame;
        kept.input = input;
        kept.state.From(state, before ? &before->state : nullptr, kBlock);
        kept.stage.From(stage, before ? &before->stage : nullptr, kBlock);
        bytes_ += Own(kept, before);
        kept_.push_back(std::move(kept));
        while (kept_.size() > 1 && (kept_.size() > most || bytes_ > mostBytes)) {
            bytes_ -= Own(kept_[0], &kept_[1]); // (what the oldest has that the next has not)
            kept_.pop_front();
        }
    }

    // The newest taken out (false: none kept)
    bool Pop(Kept &out) {
        if (kept_.empty()) {
            return false;
        }
        bytes_ -= Own(kept_.back(), kept_.size() > 1 ? &kept_[kept_.size() - 2] : nullptr);
        out = std::move(kept_.back());
        kept_.pop_back();
        return true;
    }

    // Only the kept states before `frame` (the ones from it on are what happened before going back to it)
    void DropFrom(uint64_t frame) {
        Kept dropped;
        while (!kept_.empty() && kept_.back().frame >= frame) {
            Pop(dropped);
        }
    }

private:
    static size_t Own(const Kept &kept, const Kept *other) {
        return kept.state.Own(other ? &other->state : nullptr) + kept.stage.Own(other ? &other->stage : nullptr);
    }
    std::deque<Kept> kept_;
    size_t bytes_ = 0;
};

} // namespace port

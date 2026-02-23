#pragma once

#include "ll/api/mod/NativeMod.h"

namespace optimized_chunk_ticking {

class OptimizedChunkTicking {
public:
    static OptimizedChunkTicking& getInstance();

    OptimizedChunkTicking() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();
    bool enable();
    bool disable();

private:
    ll::mod::NativeMod& mSelf;
};

} // namespace optimized_chunk_ticking

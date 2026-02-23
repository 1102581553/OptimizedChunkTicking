#include "OptimizedChunkTicking.h"

// 禁用 C4996 警告（$tick 虽标记不可用但实际存在）
#pragma warning(disable: 4996)

#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "ll/api/mod/RegisterHelper.h"

#include "mc/deps/ecs/EntityId.h"
#include "mc/deps/ecs/gamerefs_entity/EntityRegistry.h"
#include "mc/deps/ecs/gamerefs_entity/EntityContext.h"
#include "mc/deps/vanilla_components/ActorComponent.h"
#include "mc/entity/systems/LevelChunkTickingSystem.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/ChunkPos.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/chunk/LevelChunk.h"

#include <unordered_map>
#include <unordered_set>

namespace optimized_chunk_ticking {

LL_AUTO_TYPE_INSTANCE_HOOK(
    LevelChunkTickingSystemTickHook,
    ll::memory::HookPriority::Normal,
    LevelChunkTickingSystem,
    &LevelChunkTickingSystem::$tick,
    void,
    ::EntityRegistry& registry
) {
    auto level = ll::service::getLevel();
    if (!level) {
        return origin(registry);
    }

    std::unordered_map<DimensionType, std::unordered_set<ChunkPos>> chunksToTick;
    std::unordered_map<DimensionType, BlockSource*> dimToRegion;

    auto& enttRegistry = registry.mRegistry.get();
    auto view = enttRegistry.template view<ActorComponent>();
    for (auto entity : view) {
        // 使用指定初始化器明确初始化每个成员
        EntityContext ctx{
            .mRegistry = registry,
            .mEnTTRegistry = enttRegistry,
            .mEntity = entity
        };
        Actor* actor = Actor::tryGetFromEntity(ctx, false);
        if (!actor) continue;

        DimensionType dimId = actor->getDimensionId();
        BlockSource& region = actor->getDimensionBlockSource();

        if (dimToRegion.find(dimId) == dimToRegion.end()) {
            dimToRegion[dimId] = &region;
        }

        uint range = region.getLevel().getChunkTickRange();
        Vec3 pos = actor->getPosition();
        ChunkPos center((int)std::floor(pos.x) >> 4, (int)std::floor(pos.z) >> 4);

        for (int dx = -static_cast<int>(range); dx <= static_cast<int>(range); ++dx) {
            for (int dz = -static_cast<int>(range); dz <= static_cast<int>(range); ++dz) {
                ChunkPos cp(center.x + dx, center.z + dz);
                if (region.getChunk(cp) != nullptr) {
                    chunksToTick[dimId].insert(cp);
                }
            }
        }
    }

    for (auto& [dimId, chunkSet] : chunksToTick) {
        BlockSource* region = dimToRegion[dimId];
        if (!region) continue;
        for (const ChunkPos& cp : chunkSet) {
            LevelChunk* chunk = region->getChunk(cp);
            if (chunk) {
                chunk->tickBlocks(*region);
                chunk->tickBlockEntities(*region);
            }
        }
    }
}

bool OptimizedChunkTicking::load() {
    getSelf().getLogger().info("Loading OptimizedChunkTicking...");
    return true;
}

bool OptimizedChunkTicking::enable() {
    getSelf().getLogger().info("Enabling OptimizedChunkTicking...");
    return true;
}

bool OptimizedChunkTicking::disable() {
    getSelf().getLogger().info("Disabling OptimizedChunkTicking...");
    return true;
}

} // namespace optimized_chunk_ticking

LL_REGISTER_MOD(optimized_chunk_ticking::OptimizedChunkTicking, optimized_chunk_ticking::OptimizedChunkTicking::getInstance());

#include "OptimizedChunkTicking.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "ll/api/mod/RegisterHelper.h"

#include "mc/deps/ecs/EntityId.h"
#include "mc/deps/ecs/EntityRegistry.h"
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

// 自动注册的 Hook，替换 LevelChunkTickingSystem::tick
LL_AUTO_TYPE_INSTANCE_HOOK(
    LevelChunkTickingSystemTickHook,
    ll::memory::HookPriority::Normal,
    LevelChunkTickingSystem,
    &LevelChunkTickingSystem::tick,
    void,
    ::EntityRegistry& registry
) {
    // 获取当前 Level（可能为空，则回退到原函数）
    auto level = ll::service::getLevel();
    if (!level) {
        return origin(registry);
    }

    // 存储需要 tick 的区块，按维度分组去重
    std::unordered_map<DimensionType, std::unordered_set<ChunkPos>> chunksToTick;
    std::unordered_map<DimensionType, BlockSource*> dimToRegion;

    // 遍历所有拥有 ActorComponent 的实体
    auto view = registry.mRegistry.template view<ActorComponent>();
    for (auto entity : view) {
        Actor* actor = Actor::tryGetFromEntity(entity, registry, false);
        if (!actor) continue;

        DimensionType dimId = actor->getDimensionId();
        BlockSource& region = actor->getDimensionBlockSource();

        // 缓存每个维度的 BlockSource
        if (dimToRegion.find(dimId) == dimToRegion.end()) {
            dimToRegion[dimId] = &region;
        }

        // 获取区块 tick 半径（模拟距离）
        uint range = region.getLevel().getChunkTickRange();
        Vec3 pos = actor->getPosition();
        ChunkPos center((int)std::floor(pos.x) >> 4, (int)std::floor(pos.z) >> 4);

        // 遍历周围区块
        for (int dx = -range; dx <= range; ++dx) {
            for (int dz = -range; dz <= range; ++dz) {
                ChunkPos cp(center.x + dx, center.z + dz);
                if (region.getChunk(cp) != nullptr) {
                    chunksToTick[dimId].insert(cp);
                }
            }
        }
    }

    // 执行区块 tick
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

    // 原函数被完全替换，不调用 origin
}

// 模组生命周期
bool OptimizedChunkTicking::load() {
    getSelf().getLogger().info("Loading OptimizedChunkTicking...");
    return true;
}

bool OptimizedChunkTicking::enable() {
    getSelf().getLogger().info("Enabling OptimizedChunkTicking...");
    // Hook 已自动注册
    return true;
}

bool OptimizedChunkTicking::disable() {
    getSelf().getLogger().info("Disabling OptimizedChunkTicking...");
    // 自动注册的 Hook 会在模组卸载时自动清理
    return true;
}

} // namespace optimized_chunk_ticking

LL_REGISTER_MOD(optimized_chunk_ticking::OptimizedChunkTicking, optimized_chunk_ticking::OptimizedChunkTicking::getInstance());

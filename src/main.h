#pragma once

#include <base/log.h>
#include <hook.h>
#include <yaml.h>
#include <base/base.h>
#include <Actor/ServerPlayer.h>
#include <Level/Level.h>
#include <Level/DimensionID.h>
#include <Level/GameRules.h>
#include <Container/PlayerInventory.h>
#include <Item/ItemStack.h>
#include <Item/ItemRegistry.h>
#include <Item/Enchant.h>
#include <BlockActor/BlockActor.h>
#include <BlockActor/ChestBlockActor.h>
#include <Block/BlockSource.h>
#include <Block/VanillaBlocks.h>
#include <Math/Vec3.h>
#include <Math/BlockPos.h>

struct itemToAdd {

    int32_t id                                       = 0;
    int32_t aux                                      = 0;
    int32_t count                                    = 0;
    std::string customName                           = "";
    std::vector<std::string> lore                    = {};
    std::vector<std::map<int32_t, int32_t>> enchants = {};

    template <typename IO> static inline bool io(IO f, itemToAdd &settings, YAML::Node &node) {
        return f(settings.id, node["id"]) &&
            f(settings.aux, node["aux"]) &&
            f(settings.count, node["count"]) &&
            f(settings.customName, node["customName"]) &&
            f(settings.lore, node["lore"]) && 
            f(settings.enchants, node["enchants"]);
    }
};

namespace YAML {
template <> struct convert<itemToAdd> {
    static Node encode(itemToAdd const &rhs) {
        Node node;
        node["id"]         = rhs.id;
        node["aux"]        = rhs.aux;
        node["count"]      = rhs.count;
        node["customName"] = rhs.customName;
        node["lore"]       = rhs.lore;
        node["enchants"]   = rhs.enchants;
        return node;
    }

    static bool decode(Node const &node, itemToAdd &rhs) {

        if (!node.IsMap()) { return false; }

        rhs.id         = node["id"].as<int32_t>();
        rhs.aux        = node["aux"].as<int32_t>();
        rhs.count      = node["count"].as<int32_t>();
        rhs.customName = node["customName"].as<std::string>();
        rhs.lore       = node["lore"].as<std::vector<std::string>>();
        rhs.enchants   = node["enchants"].as<std::vector<std::map<int32_t, int32_t>>>();
        return true;
    }
};
} // namespace YAML

inline struct Settings {

    bool enableExtraItems             = false;
    std::vector<itemToAdd> extraItems = {itemToAdd()};

    template <typename IO> static inline bool io(IO f, Settings &settings, YAML::Node &node) {
        return f(settings.enableExtraItems, node["enableExtraItems"]) && f(settings.extraItems, node["extraItems"]);
    }
} settings;

namespace ChestGravestone {

bool isSafeBlock(const Block &block, bool isAboveBlock);
bool isSafeRegion(const BlockSource &region, int32_t leadX, int32_t leadY, int32_t leadZ);
std::pair<BlockPos, BlockPos> tryGetSafeChestGravestonePos(const Player &player);
void transferPlayerInventoryToChest(Player &player, Container &chestContainer);
void tryAddYAMLItemStacksToChest(Container &chestContainer);

} // namespace ChestGravestone

DEF_LOGGER("ChestGravestone");
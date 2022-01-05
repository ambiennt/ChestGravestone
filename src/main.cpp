#include "main.h"
#include <dllentry.h>

void dllenter() {}
void dllexit() {}

DEFAULT_SETTINGS(settings);

THook(void, "?die@ServerPlayer@@UEAAXAEBVActorDamageSource@@@Z", ServerPlayer *player, ActorDamageSource *source) {

    auto* gr = &LocateService<Level>()->getGameRules();
    GameRulesIndex keepInventoryId = GameRulesIndex::KeepInventory;
    bool isKeepInventory = CallServerClassMethod<bool>("?getBool@GameRules@@QEBA_NUGameRuleId@@@Z", gr, &keepInventoryId);

    if (player->isPlayerInitialized() && !isKeepInventory) {

        // check valid position bounds
        auto newPos = player->getPos();
        newPos.y -= 1.62f;

        auto dimId = ((DimensionIds)player->mDimensionId);
        switch (dimId) {
            
            case DimensionIds::Overworld: {   
                const auto& generator = LocateService<Level>()->GetLevelDataWrapper()->getWorldGenerator();
                float lowerBounds = (generator == GeneratorType::Flat ? 1.0f : 5.0f);
                    
                if (newPos.y > 255.0f) newPos.y = 255.0f;
                else if (newPos.y < lowerBounds) newPos.y = lowerBounds;

                break;
            }

            case DimensionIds::Nether: {   
                if (newPos.y > 122.0f) newPos.y = 122.0f;
                else if (newPos.y < 5.0f) newPos.y = 5.0f;

                break;
            }

            case DimensionIds::TheEnd: {   
                if (newPos.y > 255.0f) newPos.y = 255.0f;
                else if (newPos.y < 0.0f) newPos.y = 0.0f;

                break;
            }

            default: break;
        }

        // set double chest
        BlockPos bp;
        const auto region = player->mRegion;
        auto normalizedChestPos_1 = bp.getBlockPos(newPos);
        region->setBlock(normalizedChestPos_1, *VanillaBlocks::mChest, 3, nullptr);
        newPos.x += 1.0f;
        auto normalizedChestPos_2 = bp.getBlockPos(newPos);
        region->setBlock(normalizedChestPos_2, *VanillaBlocks::mChest, 3, nullptr);

        auto chestBlock_1 = region->getBlockEntity(normalizedChestPos_1);
        auto chestBlock_2 = region->getBlockEntity(normalizedChestPos_2);
        auto chestContainer = chestBlock_1->getContainer();

        // clear curse of vanishing items and get inventory
        player->clearVanishEnchantedItems();
        auto playerInventory = player->mInventory->inventory.get();

        // copy player inventory to chest
        const int playerArmorSlots = 4;
        for (int i = 0; i < playerArmorSlots; i++) {
            auto armorItem = player->getArmor((ArmorSlot) i);
            chestContainer->addItemToFirstEmptySlot(armorItem);
            player->setArmor((ArmorSlot) i, ItemStack::EMPTY_ITEM);
        }

        auto offhandItem = player->getOffhandSlot();
        chestContainer->addItemToFirstEmptySlot(*offhandItem);
        player->setOffhandSlot(ItemStack::EMPTY_ITEM);

        const int playerInventorySlots = playerInventory->getContainerSize();
        for (int i = 0; i < playerInventorySlots; i++) {
            auto inventoryItem = playerInventory->getItem(i);
            chestContainer->addItemToFirstEmptySlot(inventoryItem);
            playerInventory->setItem(i, ItemStack::EMPTY_ITEM);
        }

        auto playerUIItem = player->getPlayerUIItem();
        chestContainer->addItemToFirstEmptySlot(playerUIItem);
        player->setPlayerUIItem(PlayerUISlot::CursorSelected, ItemStack::EMPTY_ITEM);

        // add extra items from yaml settings
        if (settings.enableExtraItems && !settings.extraItems.empty()) {
            
            for (auto &it : settings.extraItems) {

                CommandItem cmi;
                cmi.mId = it.id;
                ItemStack currentExtraItem;

                // ensure valid ranges
                it.count = std::clamp(it.count, 0, 32767);
                it.aux = std::clamp(it.aux, 0, 32767);

                // hack - so we can test for valid item ID and get its stack size outside of loop
                cmi.createInstanceWithoutCommand(&currentExtraItem, 0, it.aux, false);

                int maxStackSize = currentExtraItem.getMaxStackSize();
                int countNew = std::min(it.count, maxStackSize * playerInventorySlots);

                if (!currentExtraItem.isNull()) {

                    while (countNew > 0) {

                        int currentStack = std::min(maxStackSize, countNew);
                        cmi.createInstanceWithoutCommand(&currentExtraItem, currentStack, it.aux, false);
                        countNew -= currentStack;

                        if (!it.customName.empty()) {
                            currentExtraItem.setCustomName(it.customName);
                        }

                        if (!it.lore.empty()) {
                            currentExtraItem.setCustomLore(it.lore);
                        }

                        if (!it.enchants.empty()) {

                            for (auto &enchantList : it.enchants) {

                                for (auto &enchant : enchantList) {

                                    EnchantmentInstance instance;
                                    instance.type  = (Enchant::Type) std::clamp(enchant.first, 0, 36); // more valid range checks
                                    instance.level = std::clamp(enchant.second, -32768, 32767);
                                    EnchantUtils::applyEnchant(currentExtraItem, instance, true);
                                }
                            }
                        }
                        chestContainer->addItemToFirstEmptySlot(currentExtraItem);
                    }
                }
            }
        }

        std::string chestName = player->mPlayerName + "'s Gravestone";
        chestBlock_1->setCustomName(chestName);
        chestBlock_2->setCustomName(chestName);
        
        ((ChestBlockActor*)chestBlock_1)->mNotifyPlayersOnChange = true;
        chestBlock_1->onChanged(*region);

        // avoid unnecessary call of ServerPlayer::clearVanishEnchantedItems and Player::dropEquipment
        return CallServerClassMethod<void>("?die@Player@@UEAAXAEBVActorDamageSource@@@Z", player, source);
    }
    original(player, source);
}
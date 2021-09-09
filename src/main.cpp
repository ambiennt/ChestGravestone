#include "main.h"
#include <dllentry.h>

void dllenter() {}
void dllexit() {}
void PreInit() {}
void PostInit() {}

namespace VanillaBlocks {
    MCAPI extern Block const *mChest;
}

BlockPos getBlockPos(Vec3 currPos) {
    BlockPos pos;
    CallServerClassMethod<void>("??0BlockPos@@QEAA@AEBVVec3@@@Z", &pos, currPos);
    return pos;
}

THook(void, "?die@ServerPlayer@@UEAAXAEBVActorDamageSource@@@Z", Player *player, ActorDamageSource *source) {

    //test keepinventory
    auto gr = CallServerClassMethod<GameRules*>("?getGameRules@Level@@QEAAAEAVGameRules@@XZ", LocateService<Level>());
    const int keepInventoryId = 11;
    if (!CallServerClassMethod<bool>("?getBool@GameRules@@QEBA_NUGameRuleId@@@Z", gr, &keepInventoryId)) {

        auto newPos = player->getPos();
        newPos.y -= 1.62;
        if (newPos.y >= 0.0 && newPos.y <= 255.0) {

            //set double chest
            auto region = direct_access<BlockSource*>(player, 0x320);
            auto normalizedChestPos_1 = getBlockPos(newPos);
            region->setBlock(normalizedChestPos_1, *VanillaBlocks::mChest, 3, nullptr);
            newPos.x += 1.0;
            auto normalizedChestPos_2 = getBlockPos(newPos);
            region->setBlock(normalizedChestPos_2, *VanillaBlocks::mChest, 3, nullptr);
            auto chestBlock_1 = region->getBlockEntity(normalizedChestPos_1);
            auto chestContainer = chestBlock_1->getContainer();

            //clear curse of vanishing items and get inventory
            CallServerClassMethod<void>("?clearVanishEnchantedItems@ServerPlayer@@UEAAXXZ", player);
            Inventory* playerInventory = CallServerClassMethod<PlayerInventory*>("?getSupplies@Player@@QEAAAEAVPlayerInventory@@XZ", player)->inventory.get();

            //copy player inventory to chest
            const int playerArmorSlots = 4;
            for (int i = 0; i < playerArmorSlots; i++) {
                auto armorItem = CallServerClassMethod<ItemStack const &>("?getArmor@Actor@@UEBAAEBVItemStack@@W4ArmorSlot@@@Z", player, (ArmorSlot)i);
                if (armorItem) {
                    chestContainer->addItemToFirstEmptySlot(armorItem);
                    player->setArmor((ArmorSlot)i, ItemStack::EMPTY_ITEM);
                }
            }

            auto offhandItem = CallServerClassMethod<ItemStack*>("?getOffhandSlot@Actor@@QEBAAEBVItemStack@@XZ", player);
            if (offhandItem) {
                chestContainer->addItemToFirstEmptySlot(*offhandItem);
                player->setOffhandSlot(ItemStack::EMPTY_ITEM);
            }

            int playerInventorySlots = CallServerClassMethod<int>("?getContainerSize@FillingContainer@@UEBAHXZ", playerInventory);
            for (int i = 0; i < playerInventorySlots; i++) {
                auto inventoryItem = CallServerClassMethod<ItemStack const &>("?getItem@FillingContainer@@UEBAAEBVItemStack@@H@Z", playerInventory, i);
                if (inventoryItem) {
                    chestContainer->addItemToFirstEmptySlot(inventoryItem);
                }
            }
            playerInventory->removeAllItems();

            auto playerSelectedUIItem = CallServerClassMethod<ItemStack const &>(
                "?getItem@SimpleContainer@@UEBAAEBVItemStack@@H@Z", &direct_access<SimpleContainer>(player, 0x1078), PlayerUISlot::CursorSelected);
            if (playerSelectedUIItem) {
                chestContainer->addItemToFirstEmptySlot(playerSelectedUIItem);
                player->setPlayerUIItem(PlayerUISlot::CursorSelected, ItemStack::EMPTY_ITEM);
            }

            //mNotifyPlayersOnChange - update chest blocks to all clients
            direct_access<bool>(chestBlock_1, 0x278) = true;
            CallServerClassMethod<void>("?onChanged@ChestBlockActor@@UEAAXAEAVBlockSource@@@Z", chestBlock_1, region);

            //avoid unnecessary call of ServerPlayer::clearVanishEnchantedItems and Player::dropEquipment
            return CallServerClassMethod<void>("?die@Player@@UEAAXAEBVActorDamageSource@@@Z", player, source);
        }
    }
    original(player, source);
}
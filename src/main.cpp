#include "main.h"
#include <dllentry.h>

void dllenter() {}
void dllexit() {}

BlockPos getBlockPos(Vec3 currPos) {
    BlockPos pos;
    CallServerClassMethod<void>("??0BlockPos@@QEAA@AEBVVec3@@@Z", &pos, currPos);
    return pos;
}

THook(void, "?die@ServerPlayer@@UEAAXAEBVActorDamageSource@@@Z", ServerPlayer *player, ActorDamageSource *source) {

    auto gr = CallServerClassMethod<GameRules*>("?getGameRules@Level@@QEAAAEAVGameRules@@XZ", LocateService<Level>());
    GameRuleIds keepInventoryId = GameRuleIds::KeepInventory;

    if (player->isPlayerInitialized() && !CallServerClassMethod<bool>("?getBool@GameRules@@QEBA_NUGameRuleId@@@Z", gr, &keepInventoryId)) {

        auto newPos = player->getPos();
        newPos.y -= 1.62;
        if (newPos.y < 0.0) {
            newPos.y = 0.0;
        }
        else if (newPos.y > 255.0) {
            newPos.y = 255.0;
        }

        //set double chest
        auto region = direct_access<BlockSource*>(player, 0x320);
        auto normalizedChestPos_1 = getBlockPos(newPos);
        region->setBlock(normalizedChestPos_1, *VanillaBlocks::mChest, 3, nullptr);
        newPos.x += 1.0;
        auto normalizedChestPos_2 = getBlockPos(newPos);
        region->setBlock(normalizedChestPos_2, *VanillaBlocks::mChest, 3, nullptr);
        auto chestBlock_1 = region->getBlockEntity(normalizedChestPos_1);
        auto chestBlock_2 = region->getBlockEntity(normalizedChestPos_2);

        auto chestContainer = chestBlock_1->getContainer();

        //clear curse of vanishing items and get inventory
        player->clearVanishEnchantedItems();
        Inventory* playerInventory = CallServerClassMethod<PlayerInventory*>(
            "?getSupplies@Player@@QEAAAEAVPlayerInventory@@XZ", player)->inventory.get();

        //copy player inventory to chest
        const int playerArmorSlots = 4;
        for (int i = 0; i < playerArmorSlots; i++) {
            auto armorItem = player->getArmor((ArmorSlot) i);
            if (armorItem) {
                chestContainer->addItemToFirstEmptySlot(armorItem);
                player->setArmor((ArmorSlot) i, ItemStack::EMPTY_ITEM);
            }
        }

        auto offhandItem = player->getOffhandSlot();
        if (offhandItem) {
            chestContainer->addItemToFirstEmptySlot(*offhandItem);
            player->setOffhandSlot(ItemStack::EMPTY_ITEM);
        }

        int playerInventorySlots = playerInventory->getContainerSize();
        for (int i = 0; i < playerInventorySlots; i++) {
            auto inventoryItem = playerInventory->getItem(i);
            if (inventoryItem) {
                chestContainer->addItemToFirstEmptySlot(inventoryItem);
            }
        }
        playerInventory->removeAllItems();

        auto playerSelectedUIItem = player->getPlayerUIItem();
        if (playerSelectedUIItem) {
            chestContainer->addItemToFirstEmptySlot(playerSelectedUIItem);
            player->setPlayerUIItem(PlayerUISlot::CursorSelected, ItemStack::EMPTY_ITEM);
        }

        std::string chestName = player->getEntityName() + "'s Gravestone";
        chestBlock_1->setCustomName(chestName);
        chestBlock_2->setCustomName(chestName);
        direct_access<bool>(chestBlock_1, 0x278) = true; // mNotifyPlayersOnChange - update chest blocks to all clients
        CallServerClassMethod<void>("?onChanged@ChestBlockActor@@UEAAXAEAVBlockSource@@@Z", chestBlock_1, region);

        //avoid unnecessary call of ServerPlayer::clearVanishEnchantedItems and Player::dropEquipment
        return CallServerClassMethod<void>("?die@Player@@UEAAXAEBVActorDamageSource@@@Z", player, source);   
    }
    original(player, source);
}
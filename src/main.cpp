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
    bool isKeepInventory = CallServerClassMethod<bool>("?getBool@GameRules@@QEBA_NUGameRuleId@@@Z", gr, &keepInventoryId);

    if (!isKeepInventory) {

        //set double chest
        auto region = direct_access<BlockSource*>(player, 0x320);
        auto newPos(player->getPos());
        newPos.y -= 1.62;
        auto normalizedChestPos_1 = getBlockPos(newPos);
        region->setBlock(normalizedChestPos_1, *VanillaBlocks::mChest, 3, nullptr);
        newPos.x += 1.0;
        auto normalizedChestPos_2 = getBlockPos(newPos);
        region->setBlock(normalizedChestPos_2, *VanillaBlocks::mChest, 3, nullptr);

        //remove items with curse of vanishing
        CallServerClassMethod<void>("?clearVanishEnchantedItems@ServerPlayer@@UEAAXXZ", player);

        //get entire player inventory contents
        //fixed typo in PlayerInventory.h, "invectory" -> "inventory"
        Inventory* playerInventory = CallServerClassMethod<PlayerInventory*>("?getSupplies@Player@@QEAAAEAVPlayerInventory@@XZ", player)->inventory.get();

        auto chestBlock_1 = region->getBlockEntity(normalizedChestPos_1);
        auto chestBlock_2 = region->getBlockEntity(normalizedChestPos_2);
        auto chestContainer = chestBlock_1->getContainer();

        //copy player inventory to chest
        const int playerArmorSlots = 4;
        for (int i = 0; i < playerArmorSlots; i++) {
            auto armorItem = CallServerClassMethod<ItemStack const &>("?getArmor@Actor@@UEBAAEBVItemStack@@W4ArmorSlot@@@Z", player, (ArmorSlot)i);
            if (armorItem) {
                chestContainer->addItemToFirstEmptySlot(armorItem);
                player->setArmor((ArmorSlot)i, ItemStack());
            }
        }

        auto offhandItem = CallServerClassMethod<ItemStack*>("?getOffhandSlot@Actor@@QEBAAEBVItemStack@@XZ", player);
        if (offhandItem) {
            chestContainer->addItemToFirstEmptySlot(*offhandItem);
            player->setOffhandSlot(ItemStack());
        }

        int playerInventorySlots = CallServerClassMethod<int>("?getContainerSize@FillingContainer@@UEBAHXZ", playerInventory);
        for (int i = 0; i < playerInventorySlots; i++) {
            auto inventoryItem = CallServerClassMethod<ItemStack const &>("?getItem@FillingContainer@@UEBAAEBVItemStack@@H@Z", playerInventory, i);
            if (inventoryItem) {
                chestContainer->addItemToFirstEmptySlot(inventoryItem);
            }
        }
        playerInventory->removeAllItems();
        CallServerClassMethod<void>("?sendInventory@ServerPlayer@@UEAAX_N@Z", player, false);

        //update chest blocks to client
        auto pkt_1 = CallServerClassMethod<std::unique_ptr<BlockActorDataPacket>>(
            "?_getUpdatePacket@ChestBlockActor@@MEAA?AV?$unique_ptr@VBlockActorDataPacket@@U?$default_delete@VBlockActorDataPacket@@@std@@@std@@AEAVBlockSource@@@Z",
            chestBlock_1, normalizedChestPos_1);
        auto pkt_2 = CallServerClassMethod<std::unique_ptr<BlockActorDataPacket>>(
            "?_getUpdatePacket@ChestBlockActor@@MEAA?AV?$unique_ptr@VBlockActorDataPacket@@U?$default_delete@VBlockActorDataPacket@@@std@@@std@@AEAVBlockSource@@@Z",
            chestBlock_2, normalizedChestPos_2);
        player->sendNetworkPacket(*pkt_1);
        player->sendNetworkPacket(*pkt_2);

        //avoid unnecessary inventory call of ServerPlayer::clearVanishEnchantedItems and Player::dropEquipment
        CallServerClassMethod<void>("?die@Player@@UEAAXAEBVActorDamageSource@@@Z", player, source);
        return;
    }
    original(player, source);
}
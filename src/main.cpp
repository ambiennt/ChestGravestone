#include "main.h"
#include <dllentry.h>

void dllenter() {}
void dllexit() {}

DEFAULT_SETTINGS(settings);

namespace ChestGravestone {

bool isSafeBlock(Block const& block, bool isAboveBlock) {
	auto legacy = block.mLegacyBlock.get();
	if (!legacy) return false;
	if (isAboveBlock) {
		if (legacy->isUnbreakableBlock()) return false;
	}
	return (legacy->isAirBlock() ||
			legacy->hasBlockProperty(BlockProperty::Liquid) ||
			legacy->hasBlockProperty(BlockProperty::TopSnow) ||
			(legacy->getMaterial() == MaterialType::ReplaceablePlant));
}

bool isSafeRegion(BlockSource &region, int32_t leadX, int32_t leadY, int32_t leadZ) {
	return (isSafeBlock(region.getBlock(leadX, leadY + 1, leadZ), true) &&
			isSafeBlock(region.getBlock(leadX + 1, leadY + 1, leadZ), true) &&
			isSafeBlock(region.getBlock(leadX, leadY, leadZ), false) &&
			isSafeBlock(region.getBlock(leadX + 1, leadY, leadZ), false));
}

std::pair<BlockPos, BlockPos> tryGetSafeChestGravestonePos(Player const &player) {

	BlockPos leading(player.getPosGrounded());
	auto& region = *player.mRegion;

	if (!isSafeRegion(region, leading.x, leading.y, leading.z)) {

		// if we can't find a safe pos, just use original
		const int32_t MAX_SAFE_POS_DISPLACEMENT = 3;

		for (int32_t i = 0; i < MAX_SAFE_POS_DISPLACEMENT; i++) {
			if (isSafeRegion(region, leading.x - i, leading.y, leading.z)) {
				leading = BlockPos(leading.x - i, leading.y, leading.z);
				break;
			}
			else if (isSafeRegion(region, leading.x + i, leading.y, leading.z)) {
				leading = BlockPos(leading.x + i, leading.y, leading.z);
				break;
			}
			else if (isSafeRegion(region, leading.x, leading.y, leading.z - i)) {
				leading = BlockPos(leading.x, leading.y, leading.z - i);
				break;
			}
			else if (isSafeRegion(region, leading.x, leading.y, leading.z + i)) {
				leading = BlockPos(leading.x, leading.y, leading.z + i);
				break;
			}
			else if (isSafeRegion(region, leading.x, leading.y - i, leading.z)) {
				leading = BlockPos(leading.x, leading.y - i, leading.z);
				break;
			}
			else if (isSafeRegion(region, leading.x, leading.y + i, leading.z)) {
				leading = BlockPos(leading.x, leading.y + i, leading.z);
				break;
			}
		}
	}

	switch (player.mDimensionId) {

		case DimensionID::Overworld: {
			int32_t lowerBounds = ((player.mLevel->getWorldGeneratorType() == GeneratorType::Flat) ? 1 : 5);
			leading.y = (int32_t)std::clamp(leading.y, lowerBounds, 255);
			break;
		}
		case DimensionID::Nether: {
			leading.y = (int32_t)std::clamp(leading.y, 5, 122);
			break;
		}
		case DimensionID::TheEnd: {
			leading.y = (int32_t)std::clamp(leading.y, 0, 255);
			break;
		}
		default: break;
	}

	return std::make_pair(leading, BlockPos(leading.x + 1, leading.y, leading.z));
}

} // namespace ChestGravestonw

TInstanceHook(void, "?die@ServerPlayer@@UEAAXAEBVActorDamageSource@@@Z", ServerPlayer, ActorDamageSource const& source) {

	bool isKeepInventory = this->mLevel->getGameRuleValue<bool>(GameRulesIndex::KeepInventory);

	if (this->isPlayerInitialized() && !isKeepInventory) {

        auto [leadingChestPos, pairedChestPos] = ChestGravestone::tryGetSafeChestGravestonePos(*this);

		// set double chest
		auto& region = *this->mRegion;
		region.setBlock(leadingChestPos, *VanillaBlocks::mChest, 3, nullptr);
		region.setBlock(pairedChestPos, *VanillaBlocks::mChest, 3, nullptr);

		auto chestBlock_1 = region.getBlockEntity(leadingChestPos);
		auto chestBlock_2 = region.getBlockEntity(pairedChestPos);
		auto chestContainer = chestBlock_1->getContainer();

		// clear curse of vanishing items and get inventory
		this->clearVanishEnchantedItems();
		auto& playerInventory = this->getRawInventory();

		// copy player inventory to chest
		const int32_t playerArmorSlots = 4;
		for (int32_t i = 0; i < playerArmorSlots; i++) {
			ItemStack armorCopy(this->getArmor((ArmorSlot)i));
			chestContainer->addItemToFirstEmptySlot(armorCopy);
			this->setArmor((ArmorSlot)i, ItemStack::EMPTY_ITEM);
		}

		ItemStack offhandCopy(this->getOffhandSlot());
		chestContainer->addItemToFirstEmptySlot(offhandCopy);
		this->setOffhandSlot(ItemStack::EMPTY_ITEM);

		const int32_t playerInventorySlots = playerInventory.getContainerSize();
		for (int32_t i = 0; i < playerInventorySlots; i++) {
			ItemStack inventoryCopy(playerInventory.getItem(i));
			chestContainer->addItemToFirstEmptySlot(inventoryCopy);
			playerInventory.setItem(i, ItemStack::EMPTY_ITEM);
		}

		ItemStack UIItemCopy(this->getPlayerUIItem());
		chestContainer->addItemToFirstEmptySlot(UIItemCopy);
		this->setPlayerUIItem(PlayerUISlot::CursorSelected, ItemStack::EMPTY_ITEM);

		// add extra items from yaml settings
		if (settings.enableExtraItems && !settings.extraItems.empty()) {

			for (auto& it : settings.extraItems) {

				// ensure valid aux range
				it.aux = (int32_t)std::clamp(it.aux, 0, 32767);
				
				// hack - so we can test for valid item ID and get its stack size outside of loop
				// we need to pass in the aux to the temp instance because different auxes have different stack sizes
				CommandItem cmi(it.id);
				ItemStack currentExtraItem;
				cmi.createInstance(&currentExtraItem, 0, it.aux, false);

				if (!currentExtraItem.isNull()) {

					int32_t maxStackSize = (int32_t)currentExtraItem.getMaxStackSize();
					int32_t countNew = (int32_t)std::min(it.count, maxStackSize * playerInventorySlots); // ensure valid count range

					while (countNew > 0) {

						int32_t currentStack = (int32_t)std::min(maxStackSize, countNew);
						cmi.createInstance(&currentExtraItem, currentStack, it.aux, false);
						countNew -= currentStack;

						if (!it.customName.empty()) {
							currentExtraItem.setCustomName(it.customName);
						}

						if (!it.lore.empty()) {
							currentExtraItem.setCustomLore(it.lore);
						}

						if (!it.enchants.empty()) {

							for (auto& enchantList : it.enchants) {

								for (auto& enchant : enchantList) {

									EnchantmentInstance instance;
									instance.type  = (Enchant::Type)std::clamp(enchant.first, 0, 36); // more valid range checks
									instance.level = (int32_t)std::clamp(enchant.second, -32768, 32767);
									EnchantUtils::applyEnchant(currentExtraItem, instance, true);
								}
							}
						}
						// so the extra item can stack if dead player already had this item in their inventory
						chestContainer->addItem(currentExtraItem);
					}
				}
			}
		}

		std::string chestName = this->mPlayerName + "'s Gravestone";
		chestBlock_1->setCustomName(chestName);
		chestBlock_2->setCustomName(chestName);

		((ChestBlockActor*)chestBlock_1)->mNotifyPlayersOnChange = true;
		chestBlock_1->onChanged(region);

		// avoid unnecessary call of ServerPlayer::clearVanishEnchantedItems and Player::dropEquipment
		// return ((Player*)this)->die(source); // WHY DOES THIS CRASH
		return CallServerClassMethod<void>("?die@Player@@UEAAXAEBVActorDamageSource@@@Z", this, source);
	}
	original(this, source);
}
#include "main.h"
#include <dllentry.h>

void dllenter() {}
void dllexit() {}

DEFAULT_SETTINGS(settings);

bool ChestGravestone::isSafeBlock(const Block &block, bool isAboveBlock) {
	if (!block.mLegacyBlock) return false;
	auto& legacy = *block.mLegacyBlock.get();
	if (isAboveBlock) {
		if (legacy.isUnbreakableBlock()) return false;
	}
	return (legacy.isAirBlock() ||
			legacy.hasBlockProperty(BlockProperty::Liquid) ||
			legacy.hasBlockProperty(BlockProperty::TopSnow) ||
			(legacy.getMaterialType() == MaterialType::ReplaceablePlant));
}

bool ChestGravestone::isSafeRegion(const BlockSource &region, int32_t leadX, int32_t leadY, int32_t leadZ) {
	return (isSafeBlock(region.getBlock(leadX, leadY + 1, leadZ), true) &&
			isSafeBlock(region.getBlock(leadX + 1, leadY + 1, leadZ), true) &&
			isSafeBlock(region.getBlock(leadX, leadY, leadZ), false) &&
			isSafeBlock(region.getBlock(leadX + 1, leadY, leadZ), false));
}

std::pair<BlockPos, BlockPos> ChestGravestone::tryGetSafeChestGravestonePos(const Player &player) {

	BlockPos leading = player.getBlockPosGrounded();
	auto& region = *player.mRegion;

	if (!isSafeRegion(region, leading.x, leading.y, leading.z)) {

		// if we can't find a safe pos, just use original
		constexpr int32_t MAX_SAFE_POS_DISPLACEMENT = 3;

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

void ChestGravestone::transferPlayerInventoryToChest(Player &player, Container& chestContainer) {

	constexpr int32_t PLAYER_ARMOR_SLOT_COUNT = 4;
	for (int32_t i = 0; i < PLAYER_ARMOR_SLOT_COUNT; i++) {
		ItemStack armorCopy(player.getArmor((ArmorSlot)i));
		chestContainer.addItemToFirstEmptySlot(armorCopy);
		player.setArmor((ArmorSlot)i, ItemStack::EMPTY_ITEM);
	}

	ItemStack offhandCopy(player.getOffhandSlot());
	chestContainer.addItemToFirstEmptySlot(offhandCopy);
	player.setOffhandSlot(ItemStack::EMPTY_ITEM);

	auto& playerInventory = player.getRawInventory();
	for (int32_t i = 0; i < playerInventory.getContainerSize(); i++) {
		ItemStack inventoryCopy(playerInventory.getItem(i));
		chestContainer.addItemToFirstEmptySlot(inventoryCopy);
		playerInventory.setItem(i, ItemStack::EMPTY_ITEM);
	}

	ItemStack UIItemCopy(player.getPlayerUIItem());
	chestContainer.addItemToFirstEmptySlot(UIItemCopy);
	player.setPlayerUIItem(PlayerUISlot::CursorSelected, ItemStack::EMPTY_ITEM);
}

void ChestGravestone::tryAddYAMLItemStacksToChest(Container& chestContainer) {

  	if (settings.enableExtraItems && !settings.extraItems.empty()) {

		for (const auto &it : settings.extraItems) {

	  		auto item = ItemRegistry::getItem(it.id);
	  		if (!item) continue;

	  		int32_t newAux = std::clamp(it.aux, 0, 32767);
	  		ItemStack testStack(*item, 1, newAux);
	  		if (testStack.isNull()) continue;

	  		int32_t maxStackSize = (int32_t)testStack.getMaxStackSize();
	  		int32_t countNew = (int32_t)std::min(it.count, maxStackSize * chestContainer.getContainerSize()); // ensure valid count range

	  		while (countNew > 0) {

				int32_t currentStackSize = (int32_t)std::min(maxStackSize, countNew);
				ItemStack currentStack(*item, currentStackSize, newAux);
				countNew -= currentStack;

				if (!it.customName.empty()) {
					currentStack.setCustomName(it.customName);
				}

				if (!it.lore.empty()) {
					currentStack.setCustomLore(it.lore);
				}

				if (!it.enchants.empty()) {
		  			for (const auto &enchantList : it.enchants) {
						for (const auto &[enchId, enchLevel] : enchantList) {

			  				EnchantmentInstance enchInstance(
				  				(Enchant::Type)std::clamp(enchId, 0, 36), // clamp more valid range checks
				  				(int32_t)std::clamp(enchLevel, -32768, 32767)
							);
			  				EnchantUtils::applyUnfilteredEnchant(currentStack, enchInstance, false);
						}
		  			}
				}

				// so the extra item can stack if dead player already had this item in their inventory
				chestContainer.addItem(currentStack);
	  		}
		}
  	}
}

TInstanceHook(void, "?die@ServerPlayer@@UEAAXAEBVActorDamageSource@@@Z", ServerPlayer, const ActorDamageSource &source) {

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

		// clear curse of vanishing items
		this->clearVanishEnchantedItems();

		// copy player inventory to chest
		ChestGravestone::transferPlayerInventoryToChest(*this, *chestContainer);

		// add extra items from yaml settings
		ChestGravestone::tryAddYAMLItemStacksToChest(*chestContainer);

		std::string chestName(this->mPlayerName + "'s Gravestone");
		chestBlock_1->setCustomName(chestName);
		chestBlock_2->setCustomName(chestName);

		((ChestBlockActor*)chestBlock_1)->mNotifyPlayersOnChange = true;
		chestBlock_1->onChanged(region);

		// avoid unnecessary call of ServerPlayer::clearVanishEnchantedItems and Player::dropEquipment
		CallServerClassMethod<void>("?die@Player@@UEAAXAEBVActorDamageSource@@@Z", this, source);
		return;
	}
	original(this, source);
}
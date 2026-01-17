/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

bot_exports.cpp implementation.*/

#include "../g_local.hpp"
#include "bot_exports.hpp"

/*
================
Bot_SetWeapon
================
*/
void Bot_SetWeapon(gentity_t *bot, const int weaponIndex, const bool instantSwitch) {
	if (weaponIndex <= IT_NULL || weaponIndex > IT_TOTAL) {
		return;
	}

	if ((bot->svFlags & SVF_BOT) == 0) {
		return;
	}

	gclient_t *client = bot->client;
	if (client == nullptr || !client->pers.inventory[weaponIndex]) {
		return;
	}

	const item_id_t weaponItemID = static_cast<item_id_t>(weaponIndex);

	const Item *currentGun = client->pers.weapon;
	if (currentGun != nullptr) {
		if (currentGun->id == weaponItemID) {
			return;
		} // already have the gun in hand.
	}

	const Item *pendingGun = client->weapon.pending;
	if (pendingGun != nullptr) {
		if (pendingGun->id == weaponItemID) {
			return;
		} // already in the process of switching to that gun, just be patient!
	}

	Item *item = &itemList[weaponIndex];
	if ((item->flags & IF_WEAPON) == 0) {
		return;
	}

	if (item->use == nullptr) {
		return;
	}

	bot->client->noWeaponChains = true;
	item->use(bot, item);

	if (instantSwitch) {
		// FIXME: ugly, maybe store in client later
		const bool temp_instant_weapon = g_instantWeaponSwitch->integer || !!g_frenzy->integer;
		g_instantWeaponSwitch->integer = 1;
		Change_Weapon(bot);
		g_instantWeaponSwitch->integer = temp_instant_weapon;
	}
}

/*
================
Bot_TriggerEntity
================
*/
void Bot_TriggerEntity(gentity_t *bot, gentity_t *entity) {
	if (!bot->inUse || !entity->inUse) {
		return;
	}

	if ((bot->svFlags & SVF_BOT) == 0) {
		return;
	}

	if (entity->use) {
		entity->use(entity, bot, bot);
	}

	trace_t unUsed;
	if (entity->touch) {
		entity->touch(entity, bot, unUsed, true);
	}
}

/*
================
Bot_UseItem
================
*/
void Bot_UseItem(gentity_t *bot, const int32_t itemID) {
	if (!bot->inUse) {
		return;
	}

	if ((bot->svFlags & SVF_BOT) == 0) {
		return;
	}

	const item_id_t desiredItemID = item_id_t(itemID);
	bot->client->pers.selectedItem = desiredItemID;

	ValidateSelectedItem(bot);

	if (bot->client->pers.selectedItem == IT_NULL) {
		return;
	}

	if (bot->client->pers.selectedItem != desiredItemID) {
		return;
	} // the itemID changed on us - don't use it!

	Item *item = &itemList[bot->client->pers.selectedItem];
	bot->client->pers.selectedItem = IT_NULL;

	if (item->use == nullptr) {
		return;
	}

	bot->client->noWeaponChains = true;
	item->use(bot, item);
}

/*
================
Bot_GetItemID
================
*/
int32_t Bot_GetItemID(const char *className) {
	if (className == nullptr || className[0] == '\0') {
		return Item_Invalid;
	}

	if (Q_strcasecmp(className, "none") == 0) {
		return ITEM_NULL;
	}

	for (int i = 0; i < IT_TOTAL; ++i) {
		const Item *item = &itemList[i];
		if (item->className == nullptr || item->className[0] == '\0') {
			continue;
		}

		if (Q_strcasecmp(item->className, className) == 0) {
			return item->id;
		}
	}

	return Item_Invalid;
}

/*
================
Entity_ForceLookAtPoint
================
*/
void Entity_ForceLookAtPoint(gentity_t *entity, gvec3_cref_t point) {
	Vector3 viewOrigin = entity->s.origin;
	if (entity->client != nullptr) {
		viewOrigin += entity->client->ps.viewOffset;
	}

	const Vector3 ideal = (point - viewOrigin).normalized();

	Vector3 viewAngles = VectorToAngles(ideal);
	if (viewAngles.x < -180.0f) {
		viewAngles.x = anglemod(viewAngles.x + 360.0f);
	}

	if (entity->client != nullptr) {
		entity->client->ps.pmove.deltaAngles = (viewAngles - entity->client->resp.cmdAngles);
		entity->client->ps.viewAngles = {};
		entity->client->vAngle = {};
		entity->s.angles = {};
	}
}

/*
================
Bot_PickedUpItem

Check if the given bot has picked up the given item or not.
================
*/
bool Bot_PickedUpItem(gentity_t *bot, gentity_t *item) {
	if (bot->s.number == 0 || bot->s.number > MAX_CLIENTS)
		return false; // invalid or out of range

	return item->itemPickedUpBy[static_cast<int>(bot->s.number - 1)];
}

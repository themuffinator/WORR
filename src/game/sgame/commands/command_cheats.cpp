/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

commands_cheats.cpp - Implementations for cheat and debug commands.*/

#include "../g_local.hpp"
#include "command_registration.hpp"
#include <vector>

namespace Commands {

	// --- Static Helper Functions ---

	static void PrintState(gentity_t* ent, const CommandArgs& args, bool on_state) {
		std::string_view s = args.getString(0);
		if (!s.empty())
			gi.LocClient_Print(ent, PRINT_HIGH, "{} {}\n", s.data(), on_state ? "ON" : "OFF");
	}

	static void SpawnAndGiveItem(gentity_t* ent, item_id_t id, int count = 1) {
		Item* it = GetItemByIndex(id);
		if (!it) return;

		gentity_t* it_ent = Spawn();
		it_ent->className = it->className;
		SpawnItem(it_ent, it);
		if (it->flags & IF_AMMO) {
			it_ent->count = count;
		}

		if (it_ent->inUse) {
			trace_t tr = null_trace;
			tr.ent = it_ent;
			Touch_Item(it_ent, ent, tr, true);
			if (it_ent->inUse)
				FreeEntity(it_ent);
		}
	}

	// --- Cheat & Debug Command Implementations ---

	static void AlertAll(gentity_t* ent, const CommandArgs& args) {
		for (size_t i = 0; i < globals.numEntities; i++) {
			gentity_t* t = &g_entities[i];
			if (t->inUse && t->health > 0 && (t->svFlags & SVF_MONSTER)) {
				t->enemy = ent;
				FoundTarget(t);
			}
		}
		gi.Client_Print(ent, PRINT_HIGH, "All monsters alerted.\n");
	}

	static void CheckPOI(gentity_t* ent, const CommandArgs& args) {
		if (!level.poi.valid) {
			gi.Client_Print(ent, PRINT_HIGH, "No POI set.\n");
			return;
		}
		char pvs = gi.inPVS(ent->s.origin, level.poi.current, false) ? 'Y' : 'N';
		char pvs_portals = gi.inPVS(ent->s.origin, level.poi.current, true) ? 'Y' : 'N';
		gi.Com_PrintFmt("POI Visibility Check:\n PVS: {}, PVS+Portals: {}\n", pvs, pvs_portals);
	}

	static void ClearAIEnemy(gentity_t* ent, const CommandArgs& args) {
		for (size_t i = 1; i < globals.numEntities; i++) {
			gentity_t* entity = &g_entities[i];
			if (entity->inUse && (entity->svFlags & SVF_MONSTER)) {
				entity->monsterInfo.aiFlags |= AI_FORGET_ENEMY;
			}
		}
		gi.Client_Print(ent, PRINT_HIGH, "Cleared all AI enemies.\n");
	}

	static void Give(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2) {
			PrintUsage(ent, args, "<item_name|all|health|...>", "[count]", "Gives an item to the player.");
			return;
		}
		std::string_view name = args.getString(1);

		if (name == "all") {
			ent->health = ent->maxHealth;
			for (int i = 0; i < IT_TOTAL; ++i) {
				Item* it = &itemList[i];
				if (!it->pickup || (it->flags & IF_NOT_GIVEABLE)) continue;
				if (it->flags & IF_WEAPON) ent->client->pers.inventory[i] += 1;
				if (it->flags & IF_AMMO) Add_Ammo(ent, it, 999);
			}
			ent->client->pers.inventory[IT_ARMOR_BODY] = armor_stats[static_cast<int>(game.ruleset)][Armor::Body].max_count;
			return;
		}

		Item* it = FindItem(name.data());
		if (!it) it = FindItemByClassname(name.data());

		if (!it) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_unknown_item_name", name.data());
			return;
		}
		if (it->flags & IF_NOT_GIVEABLE) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_not_giveable");
			return;
		}

		int count = args.getInt(2).value_or(1);
		SpawnAndGiveItem(ent, it->id, count);
	}

	static void God(gentity_t* ent, const CommandArgs& args) {
		ent->flags ^= FL_GODMODE;
		PrintState(ent, args, ent->flags & FL_GODMODE);
	}

	static void Immortal(gentity_t* ent, const CommandArgs& args) {
		ent->flags ^= FL_IMMORTAL;
		PrintState(ent, args, ent->flags & FL_IMMORTAL);
	}

	static void KillAI(gentity_t* ent, const CommandArgs& args) {
		for (int i = 1; i < static_cast<int>(globals.numEntities); i++) {
			gentity_t* entity = &g_entities[i];
			if (entity->inUse && (entity->svFlags & SVF_MONSTER)) {
				FreeEntity(entity);
			}
		}
		gi.Client_Print(ent, PRINT_HIGH, "All AI have been removed.\n");
	}

	static void ListEntities(gentity_t* ent, const CommandArgs& args) {
		gi.Client_Print(ent, PRINT_HIGH, "--- Entity List ---\n");
		for (int i = 0; i < static_cast<int>(globals.numEntities); ++i) {
			gentity_t* e = &g_entities[i];
			if (e->inUse) {
				gi.Com_PrintFmt("{}: {}\n", i, e->className);
			}
		}
		gi.Client_Print(ent, PRINT_HIGH, "-------------------\n");
	}

	static void ListMonsters(gentity_t* ent, const CommandArgs& args) {
		gi.Client_Print(ent, PRINT_HIGH, "--- Monster List ---\n");
		int count = 0;
		for (int i = 0; i < static_cast<int>(globals.numEntities); ++i) {
			gentity_t* e = &g_entities[i];
			if (e->inUse && (e->svFlags & SVF_MONSTER)) {
				gi.Com_PrintFmt("{}: {} at {}\n", i, e->className, e->s.origin);
				count++;
			}
		}
		gi.Com_PrintFmt("Total monsters: {}\n", count);
		gi.Client_Print(ent, PRINT_HIGH, "--------------------\n");
	}

	static void NoClip(gentity_t* ent, const CommandArgs& args) {
		ent->moveType = (ent->moveType == MoveType::NoClip) ? MoveType::Walk : MoveType::NoClip;
		PrintState(ent, args, ent->moveType == MoveType::NoClip);
	}

	static void NoTarget(gentity_t* ent, const CommandArgs& args) {
		ent->flags ^= FL_NOTARGET;
		PrintState(ent, args, ent->flags & FL_NOTARGET);
	}

	static void NoVisible(gentity_t* ent, const CommandArgs& args) {
		ent->flags ^= FL_NOVISIBLE;
		PrintState(ent, args, ent->flags & FL_NOVISIBLE);
	}

	static void SetPOI(gentity_t* ent, const CommandArgs& args) {
		level.poi.current = ent->s.origin;
		level.poi.valid = true;
		gi.Client_Print(ent, PRINT_HIGH, "Point of Interest set to your location.\n");
	}

	static void Target(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2) {
			PrintUsage(ent, args, "<target_name>", "", "Triggers all entities with the matching 'targetname'.");
			return;
		}
		std::string_view targetName = args.getString(1);
		(void)targetName;
		UseTargets(ent, ent);
	}

	static void Teleport(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 4) {
			PrintUsage(ent, args, "<x> <y> <z>", "[pitch] [yaw] [roll]", "Teleports the player to a location.");
			return;
		}
		auto x = args.getFloat(1);
		auto y = args.getFloat(2);
		auto z = args.getFloat(3);

		if (!x || !y || !z) {
			gi.Client_Print(ent, PRINT_HIGH, "Invalid coordinates provided.\n");
			return;
		}
		Vector3 origin = { *x, *y, *z };
		Vector3 angles = ent->client->ps.viewAngles;

		if (args.count() >= 7) {
			angles[PITCH] = args.getFloat(4).value_or(angles[PITCH]);
			angles[YAW] = args.getFloat(5).value_or(angles[YAW]);
			angles[ROLL] = args.getFloat(6).value_or(angles[ROLL]);
		}

		TeleportPlayer(ent, origin, angles);
	}

	// --- Registration Function ---
	void RegisterCheatCommands() {
		using enum CommandFlag;
		RegisterCommand("alert_all", &AlertAll, AllowSpectator | CheatProtect);
		RegisterCommand("check_poi", &CheckPOI, AllowSpectator | CheatProtect);
		RegisterCommand("clear_ai_enemy", &ClearAIEnemy, CheatProtect);
		RegisterCommand("give", &Give, CheatProtect, true);
		RegisterCommand("god", &God, CheatProtect, true);
		RegisterCommand("immortal", &Immortal, CheatProtect);
		RegisterCommand("kill_ai", &KillAI, CheatProtect);
		RegisterCommand("list_entities", &ListEntities, AllowDead | AllowIntermission | AllowSpectator | CheatProtect);
		RegisterCommand("list_monsters", &ListMonsters, AllowDead | AllowIntermission | AllowSpectator | CheatProtect);
		RegisterCommand("noclip", &NoClip, CheatProtect, true);
		RegisterCommand("notarget", &NoTarget, CheatProtect, true);
		RegisterCommand("novisible", &NoVisible, CheatProtect);
		RegisterCommand("set_poi", &SetPOI, AllowSpectator | CheatProtect);
		RegisterCommand("target", &Target, AllowDead | AllowSpectator | CheatProtect);
		RegisterCommand("teleport", &Teleport, AllowSpectator | CheatProtect);
	}

} // namespace Commands

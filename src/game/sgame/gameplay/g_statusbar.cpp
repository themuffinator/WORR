/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

g_statusbar.cpp implementation.*/

#include "../g_local.hpp"
#include "g_statusbar.hpp"

// create & set the statusbar string for the current gamemode

/*
=========================
AddCombatHUD
=========================
*/
static void AddCombatHUD(statusbar_t& sb) {
	// Health, ammo, armor, selected icon
	sb.ifstat(STAT_SHOW_STATUSBAR)
		.xv(0).hnum().xv(50).pic(STAT_HEALTH_ICON)
		.ifstat(STAT_AMMO_ICON).xv(100).anum().xv(150).pic(STAT_AMMO_ICON).endifstat()
		.ifstat(STAT_ARMOR_ICON).xv(200).rnum().xv(250).pic(STAT_ARMOR_ICON).endifstat()
		.ifstat(STAT_SELECTED_ICON).xv(296).pic(STAT_SELECTED_ICON).endifstat()
		.endifstat();

	sb.yb(-50);

	// Pickup + selected item name
	sb.ifstat(STAT_SHOW_STATUSBAR)
		.ifstat(STAT_PICKUP_ICON).xv(0).pic(STAT_PICKUP_ICON).xv(26).yb(-42).loc_stat_string(STAT_PICKUP_STRING).yb(-50).endifstat()
		.ifstat(STAT_SELECTED_ITEM_NAME).yb(-34).xv(319).loc_stat_rstring(STAT_SELECTED_ITEM_NAME).yb(-58).endifstat()
		.endifstat();

	// Help icon
	sb.ifstat(STAT_SHOW_STATUSBAR).ifstat(STAT_HELPICON).xv(150).pic(STAT_HELPICON).endifstat().endifstat();
}


/*
=========================
AddPowerupsAndTech
=========================
*/
static void AddPowerupsAndTech(statusbar_t& sb) {
	sb.ifstat(STAT_SHOW_STATUSBAR)
		.ifstat(STAT_POWERUP_ICON).xv(262).num(2, STAT_POWERUP_TIME).xv(296).pic(STAT_POWERUP_ICON).endifstat()
		.ifstat(STAT_TECH).yb(-137).xr(-26).pic(STAT_TECH).endifstat()
		.endifstat();
}


/*
=========================
AddCoopStatus
=========================
*/
static void AddCoopStatus(statusbar_t& sb) {
	sb.ifstat(STAT_COOP_RESPAWN).xv(0).yt(0).loc_stat_cstring2(STAT_COOP_RESPAWN).endifstat();

	int y = 2;
	const int step = 26;
	if (G_LimitedLivesActive())
		sb.ifstat(STAT_LIVES).xr(-16).yt(y).lives_num(STAT_LIVES).xr(0).yt(y += step).loc_rstring("$g_lives").endifstat();

	if (Game::Is(GameType::Horde)) {
		int n = level.roundNumber;
		int chars = n > 99 ? 3 : n > 9 ? 2 : 1;
		sb.ifstat(STAT_ROUND_NUMBER).xr(-32 - (16 * chars)).yt(y += 10).num(3, STAT_ROUND_NUMBER).xr(0).yt(y += step).loc_rstring("Wave").endifstat();

		n = level.campaign.totalMonsters - level.campaign.killedMonsters;
		chars = n > 99 ? 3 : n > 9 ? 2 : 1;
		sb.ifstat(STAT_MONSTER_COUNT).xr(-32 - (16 * chars)).yt(y += 10).num(3, STAT_MONSTER_COUNT).xr(0).yt(y += step).loc_rstring("Monsters").endifstat();
	}
}


/*
=========================
AddSPExtras
=========================
*/
static void AddSPExtras(statusbar_t& sb) {
	// Key icons, powerup icon reflow, selected item name offset
	sb.ifstat(STAT_POWERUP_ICON).yb(-76).endifstat();
	sb.ifstat(STAT_SELECTED_ITEM_NAME)
		.yb(-58)
		.ifstat(STAT_POWERUP_ICON).yb(-84).endifstat()
		.endifstat();

	sb.ifstat(STAT_KEY_A).xv(296).pic(STAT_KEY_A).endifstat();
	sb.ifstat(STAT_KEY_B).xv(272).pic(STAT_KEY_B).endifstat();
	sb.ifstat(STAT_KEY_C).xv(248).pic(STAT_KEY_C).endifstat();

	sb.ifstat(STAT_HEALTH_BARS).yt(24).health_bars().endifstat();

	sb.story();
}


/*
=========================
AddDeathmatchStatus
=========================
*/
static void AddDeathmatchStatus(statusbar_t& sb) {
	if (Teams()) {
		if (Game::Has(GameFlags::CTF))
			sb.ifstat(STAT_CTF_FLAG_PIC).xr(-24).yt(26).pic(STAT_CTF_FLAG_PIC).endifstat();

		sb.ifstat(STAT_TEAMPLAY_INFO).xl(0).yb(-88).stat_string(STAT_TEAMPLAY_INFO).endifstat();
	}

	sb.ifstat(STAT_COUNTDOWN).xv(136).yb(-256).num(3, STAT_COUNTDOWN).endifstat();
	sb.ifstat(STAT_MATCH_STATE).xv(0).yb(-78).stat_string(STAT_MATCH_STATE).endifstat();

	sb.ifstat(STAT_FOLLOWING).xv(0).yb(-68).string2("FOLLOWING").xv(80).stat_string(STAT_FOLLOWING).endifstat();
	sb.ifstat(STAT_SPECTATOR).xv(0).yb(-68).string2("SPECTATING").xv(0).yb(-58).string("Use TAB Menu to join the match.").xv(80).endifstat();

	sb.ifstat(STAT_MINISCORE_FIRST_PIC).xr(-26).yb(-110).pic(STAT_MINISCORE_FIRST_PIC).xr(-78).num(3, STAT_MINISCORE_FIRST_SCORE).ifstat(STAT_MINISCORE_FIRST_VAL).xr(-24).yb(-94).stat_string(STAT_MINISCORE_FIRST_VAL).endifstat().endifstat();
	sb.ifstat(STAT_MINISCORE_FIRST_POS).xr(-28).yb(-112).pic(STAT_MINISCORE_FIRST_POS).endifstat();
	sb.ifstat(STAT_MINISCORE_SECOND_PIC).xr(-26).yb(-83).pic(STAT_MINISCORE_SECOND_PIC).xr(-78).num(3, STAT_MINISCORE_SECOND_SCORE).ifstat(STAT_MINISCORE_SECOND_VAL).xr(-24).yb(-68).stat_string(STAT_MINISCORE_SECOND_VAL).endifstat().endifstat();
	sb.ifstat(STAT_MINISCORE_SECOND_POS).xr(-28).yb(-85).pic(STAT_MINISCORE_SECOND_POS).endifstat();
	sb.ifstat(STAT_MINISCORE_FIRST_PIC).xr(-28).yb(-57).stat_string(STAT_SCORELIMIT).endifstat();

	sb.ifstat(STAT_CROSSHAIR_ID_VIEW).xv(122).yb(-128).stat_pname(STAT_CROSSHAIR_ID_VIEW).endifstat();
	sb.ifstat(STAT_CROSSHAIR_ID_VIEW_COLOR).xv(156).yb(-118).pic(STAT_CROSSHAIR_ID_VIEW_COLOR).endifstat();
}


/*
=========================
G_InitStatusbar
=========================
*/
void G_InitStatusbar() {
	statusbar_t sb;
	bool minhud = g_instaGib->integer || g_nadeFest->integer;

	sb.yb(-24);

	// Health block - compact for minHUD, full otherwise
	sb.ifstat(STAT_SHOW_STATUSBAR)
		.xv(minhud ? 100 : 0).hnum().xv(minhud ? 150 : 50).pic(STAT_HEALTH_ICON)
		.endifstat();

	if (!minhud) AddCombatHUD(sb);
	AddPowerupsAndTech(sb);

	if (CooperativeModeOn() || G_LimitedLivesInLMS()) AddCoopStatus(sb);
	if (!deathmatch->integer) AddSPExtras(sb);
	else AddDeathmatchStatus(sb);

	gi.configString(CS_STATUSBAR, sb.sb.str().c_str());
}

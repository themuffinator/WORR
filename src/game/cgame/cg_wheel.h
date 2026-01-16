#pragma once

#include "cg_local.h"

void CG_Wheel_Init(void);
void CG_Wheel_Precache(void);
void CG_Wheel_ParseConfigString(int32_t index, const char *s);
void CG_Wheel_Update(const player_state_t *ps);
void CG_Wheel_Draw(const player_state_t *ps, const vrect_t &hud_vrect, const vrect_t &hud_safe, int32_t scale);
void CG_Wheel_Open(bool powerup);
void CG_Wheel_Close(bool released);
void CG_Wheel_Input(int dx, int dy);
void CG_Wheel_WeapNext(void);
void CG_Wheel_WeapPrev(void);
void CG_Wheel_ApplyButtons(button_t *cmd_buttons);
void CG_Wheel_ClearInput(void);
bool CG_Wheel_IsOpen(void);
float CG_Wheel_TimeScale(void);
bool CG_Wheel_AllowAttack(void);
int32_t CG_Wheel_GetWarnAmmoCount(int32_t weapon_id);

void CG_WeaponBar_Init(void);
void CG_WeaponBar_Precache(void);
void CG_WeaponBar_Draw(const player_state_t *ps, const vrect_t &hud_vrect, const vrect_t &hud_safe, int32_t scale);
void CG_WeaponBar_Input(const player_state_t *ps, button_t *cmd_buttons);
void CG_WeaponBar_ClearInput(void);

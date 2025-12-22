/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// cl_weapon_offsets.h -- weapon-specific aim mode position offsets

#ifndef CL_WEAPON_OFFSETS_H
#define CL_WEAPON_OFFSETS_H

// Weapon model indices (from g_local.h)
#define WEAP_BLASTER			1
#define WEAP_SHOTGUN			2
#define WEAP_SUPERSHOTGUN		3
#define WEAP_MACHINEGUN			4
#define WEAP_CHAINGUN			5
#define WEAP_GRENADES			6
#define WEAP_GRENADELAUNCHER	7
#define WEAP_ROCKETLAUNCHER		8
#define WEAP_HYPERBLASTER		9
#define WEAP_RAILGUN			10
#define WEAP_BFG				11

// Weapon-specific aim mode offsets
// These values are applied when aiming (e.g., with +centerweapon command)
// Format: { gun_x, gun_y }
// gun_x: horizontal offset (positive = right, negative = left)
// gun_y: forward/backward offset (positive = forward, negative = backward)

typedef struct {
	float x;
	float y;
} weapon_aim_offset_t;

static const weapon_aim_offset_t weapon_aim_offsets[] = {
	{0.0f, 0.0f}, // [0]  - unused
	{-7.0f, -3.0f}, // [1]  - WEAP_BLASTER
	{-9.0f, -3.0f}, // [2]  - WEAP_SHOTGUN
	{-8.0f, -3.0f}, // [3]  - WEAP_SUPERSHOTGUN
	{-7.0f, -3.0f}, // [4]  - WEAP_MACHINEGUN
	{-4.0f, -3.0f}, // [5]  - WEAP_CHAINGUN
	{-7.0f, -3.0f}, // [6]  - WEAP_GRENADES
	{-7.0f, -3.0f}, // [7]  - WEAP_GRENADELAUNCHER
	{-5.0f, -3.0f}, // [8]  - WEAP_ROCKETLAUNCHER
	{-5.0f, -3.0f}, // [9]  - WEAP_HYPERBLASTER
	{-6.0f, -3.0f}, // [10] - WEAP_RAILGUN
	{-7.0f, -3.0f}, // [11] - WEAP_BFG
};

#define MAX_WEAPON_OFFSETS (sizeof(weapon_aim_offsets) / sizeof(weapon_aim_offsets[0]))

#endif // CL_WEAPON_OFFSETS_H

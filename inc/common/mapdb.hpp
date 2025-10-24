/*
Copyright (C) 2024 Jonathan "Paril" Barkley

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

typedef struct {
	char		id[32];
	char		command[32];
	char		name[64];
	bool		needsSkillSelect;
} mapdb_episode_t;

typedef struct {
	char		bsp[64];
	char		title[64];
	char		episode[32];
	char		short_name[8];
	uint8_t		unit;
	bool		sp;
	bool		dm;
	bool		bots;
	bool		ctf;
	bool		tdm;
	bool		coop;
	bool		display_bsp;
	char		*start_items;
} mapdb_map_t;

typedef struct {
	mapdb_episode_t		*episodes;
	size_t				num_episodes;
	mapdb_map_t			*maps;
	size_t				num_maps;
} mapdb_t;

const mapdb_t *MapDB_Get(void);
void MapDB_Init(void);
void MapDB_Shutdown(void);

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

#include "shared/shared.h"
#include "common/common.h"
#include "common/files.h"
#include "common/json.h"
#include "common/mapdb.h"

#define MapDB_Mallocz(size) Z_TagMallocz((size), TAG_MAPDB)

static mapdb_t mapdb;

const mapdb_t *MapDB_Get(void)
{
	return &mapdb;
}

typedef enum {
	PRIM_UNKNOWN,
	
	PRIM_U8,
	PRIM_U16,
	PRIM_U32,
	PRIM_U64,
	PRIM_I8,
	PRIM_I16,
	PRIM_I32,
	PRIM_I64,
	PRIM_FLOAT,
	PRIM_DOUBLE,
	PRIM_BOOLEAN,
	PRIM_NULL	
} mapdb_primitive_type_t;

typedef struct {
	const char				*key;
	jsmntype_t				type;
	uintptr_t				offset;
	union {
		mapdb_primitive_type_t	primitive;
		size_t                  len; // for strings
	};
} mapdb_key_t;

#define q_member_sizeof(t, m) (sizeof(((t *) 0)->m ))

static constexpr mapdb_key_t MapKeyFixedString(const char *name, size_t offset, size_t len)
{
        mapdb_key_t key{};
        key.key = name;
        key.type = JSMN_STRING;
        key.offset = offset;
        key.len = len;
        return key;
}

static constexpr mapdb_key_t MapKeyDynamicString(const char *name, size_t offset)
{
        mapdb_key_t key{};
        key.key = name;
        key.type = JSMN_STRING;
        key.offset = offset;
        key.len = 0;
        return key;
}

static constexpr mapdb_key_t MapKeySkip(const char *name)
{
        mapdb_key_t key{};
        key.key = name;
        key.type = JSMN_UNDEFINED;
        return key;
}

static constexpr mapdb_key_t MapKeyU8(const char *name, size_t offset)
{
        mapdb_key_t key{};
        key.key = name;
        key.type = JSMN_PRIMITIVE;
        key.offset = offset;
        key.primitive = PRIM_U8;
        return key;
}

static constexpr mapdb_key_t MapKeyBoolean(const char *name, size_t offset)
{
        mapdb_key_t key{};
        key.key = name;
        key.type = JSMN_PRIMITIVE;
        key.offset = offset;
        key.primitive = PRIM_BOOLEAN;
        return key;
}

#define KEY_FIXED_STRING(name) \
        MapKeyFixedString(#name, offsetof(KEY_TYPE, name), q_member_sizeof(KEY_TYPE, name))
#define KEY_DYNAMIC_STRING(name) \
        MapKeyDynamicString(#name, offsetof(KEY_TYPE, name))
#define KEY_SKIP(name) \
        MapKeySkip(#name)
#define KEY_U8(name) \
        MapKeyU8(#name, offsetof(KEY_TYPE, name))
#define KEY_BOOLEAN(name) \
        MapKeyBoolean(#name, offsetof(KEY_TYPE, name))

#define KEY_TYPE mapdb_episode_t
static const mapdb_key_t episode_keys[] = {
        KEY_FIXED_STRING(id),
        KEY_FIXED_STRING(command),
        KEY_FIXED_STRING(name),
        KEY_SKIP(activity),
        KEY_BOOLEAN(needsSkillSelect),
        {}
};
#undef KEY_TYPE

#define KEY_TYPE mapdb_map_t
static const mapdb_key_t map_keys[] = {
        KEY_FIXED_STRING(bsp),
        KEY_FIXED_STRING(title),
        KEY_FIXED_STRING(episode),
        KEY_FIXED_STRING(short_name),
        KEY_U8(unit),
        KEY_BOOLEAN(sp),
        KEY_BOOLEAN(dm),
        KEY_BOOLEAN(bots),
        KEY_BOOLEAN(ctf),
        KEY_BOOLEAN(tdm),
        KEY_BOOLEAN(coop),
        KEY_BOOLEAN(display_bsp),
        KEY_DYNAMIC_STRING(start_items),
        {}
};
#undef KEY_TYPE

static inline uint8_t *MapKeyMem(void *obj, const mapdb_key_t *key)
{
        return reinterpret_cast<uint8_t *>(obj) + key->offset;
}

static void MapDB_ParseKeys(json_parse_t *parser, void *obj, const mapdb_key_t *keys)
{
	jsmntok_t *jobj = Json_EnsureNext(parser, JSMN_OBJECT);

	for (size_t i = 0; i < jobj->size; i++) {
		bool parsed = false;

		for (const mapdb_key_t *key = keys; key->key; key++) {
			if (Json_Strcmp(parser, key->key))
				continue;

			Json_Next(parser);

			if (key->type == JSMN_UNDEFINED) {
				Json_Next(parser);
				parsed = true;
				break;
			}

			Json_Ensure(parser, key->type);

			if (key->type == JSMN_STRING) {
				size_t l = Json_Strlen(parser);
                                if (key->len) {
                                        Q_strnlcpy(reinterpret_cast<char *>(MapKeyMem(obj, key)), parser->buffer + parser->pos->start, l, key->len);
                                } else {
                                        char *buf = Z_TagMalloc(l + 1, TAG_MAPDB);

                                        if (!buf)
                                                Json_Errorno(parser, parser->pos, Q_ERR(ENOMEM));

                                        Q_strnlcpy(buf, parser->buffer + parser->pos->start, l, l + 1);
                                        *reinterpret_cast<char **>(MapKeyMem(obj, key)) = buf;
                                }
                        } else if (key->type == JSMN_PRIMITIVE) {
                                if (key->primitive == PRIM_BOOLEAN) {
                                        *reinterpret_cast<bool *>(MapKeyMem(obj, key)) = parser->buffer[parser->pos->start] == 't';
                                } else if (key->primitive == PRIM_U8) {
                                        *reinterpret_cast<uint8_t *>(MapKeyMem(obj, key)) = strtol(parser->buffer + parser->pos->start, NULL, 10);
                                } else {
                                        Json_Error(parser, parser->pos, "Unsupported primitive");
                                }
                        }

			Json_Next(parser);

			parsed = true;
			break;
		}

		if (!parsed) {
			Json_ErrorLocation(parser, parser->tokens);
			Com_DPrintf("unknown key in mapdb.json[%s]\n", parser->error_loc);
			Json_Next(parser);
			Json_SkipToken(parser);
			continue;
		}
	}
}

void MapDB_Init(void)
{
	json_parse_t parser = {0};
	
    if (Json_ErrorHandler(parser)) {
		Com_WPrintf("Failed to load/parse mapdb.json[%s]: %s\n", parser.error_loc, parser.error);
		MapDB_Shutdown();
        return;
    }
	
	Json_Load("mapdb.json", &parser);

	jsmntok_t *obj = Json_EnsureNext(&parser, JSMN_OBJECT);

	for (int i = 0; i < obj->size; i++) {
		if (Json_Strcmp(&parser, "episodes") == 0) {
			Json_Next(&parser);

			jsmntok_t *episodes = Json_EnsureNext(&parser, JSMN_ARRAY);

			mapdb.num_episodes = episodes->size;
			mapdb.episodes = Z_TagMallocz(sizeof(mapdb_episode_t) * mapdb.num_episodes, TAG_MAPDB);

			if (!mapdb.episodes)
				Json_Errorno(&parser, episodes, Q_ERR(ENOMEM));

			for (size_t i = 0; i < mapdb.num_episodes; i++)
				MapDB_ParseKeys(&parser, &mapdb.episodes[i], episode_keys);

		} else if (Json_Strcmp(&parser, "maps") == 0) {
			Json_Next(&parser);

			jsmntok_t *maps = Json_EnsureNext(&parser, JSMN_ARRAY);

			mapdb.num_maps = maps->size;
			mapdb.maps = Z_TagMallocz(sizeof(mapdb_map_t) * mapdb.num_maps, TAG_MAPDB);

			if (!mapdb.maps)
				Json_Errorno(&parser, maps, Q_ERR(ENOMEM));

			for (size_t i = 0; i < mapdb.num_maps; i++)
				MapDB_ParseKeys(&parser, &mapdb.maps[i], map_keys);

		} else {
			Json_SkipToken(&parser);
		}
	}

	Json_Free(&parser);
}

void MapDB_Shutdown(void)
{
	for (size_t i = 0; i < mapdb.num_maps; i++)
		Z_Free(mapdb.maps[i].start_items);

	Z_Free(mapdb.episodes);
	Z_Free(mapdb.maps);

	memset(&mapdb, 0, sizeof(mapdb));

    Z_LeakTest(TAG_MAPDB);
}
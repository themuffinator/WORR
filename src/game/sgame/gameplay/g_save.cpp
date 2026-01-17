/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

g_save.cpp (Game Save/Load) This file implements a modern, robust save and load system for the
game state, using JSON as the serialization format. It is designed to be forwards and backwards
compatible by using a descriptive, field-based approach rather than raw memory dumps. Key
Responsibilities: - Serialization: `WriteGameJson` and `WriteLevelJson` traverse the game state
(clients, entities) and serialize their data into a JSON object based on detailed structure
definitions (`save_struct_t`). - Deserialization: `ReadGameJson` and `ReadLevelJson` parse a
JSON string, reconstruct the game state, and correctly link entity references and function
pointers. - Type Safety and Reflection: Uses a system of templates and macros (`FIELD_AUTO`,
`SAVE_STRUCT_START`) to automatically deduce the type of each field to be saved, making the
system easy to extend and maintain. - Pointer Marshalling: Safely handles saving and restoring
function pointers and entity pointers by converting them to names or ID numbers.*/

#include <algorithm>
#include <new>
#include <sstream>

#include "../g_local.hpp"
#include "g_clients.hpp"
#include "g_save_metadata.hpp"
#ifdef __clang__
#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Weverything"
#endif
#include "json/json.h"
#include "json/config.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#endif

// new save format;
// - simple JSON format
// - work via 'type' definitions which declare the type
//   that is at the specified offset of a struct
// - backwards & forwards compatible with this same format
// - I wrote this initially when the codebase was in C, so it
//   does have some C-isms in here.

// Professor Daniel J. Bernstein; https://www.partow.net/programming/hashfunctions/#APHashFunction MIT
struct cstring_hash {
	inline std::size_t operator()(const char* str) const {
		size_t   len = strlen(str);
		uint32_t hash = 5381;
		size_t i = 0;

		for (i = 0; i < len; ++str, ++i)
			hash = ((hash << 5) + hash) + (*str);

		return hash;
	}
};

struct cstring_equal {
	inline bool operator()(const char* a, const char* b) const {
		return strcmp(a, b) == 0;
	}
};

struct ptr_tag_hash {
	inline std::size_t operator()(const std::tuple<const void*, save_data_tag_t>& v) const {
		return (std::hash<const void*>()(std::get<0>(v)) * 8747) + std::get<1>(v);
	}
};

static bool save_data_initialized = false;
static const save_data_list_t* list_head = nullptr;
static std::unordered_map<const void*, const save_data_list_t*> list_hash;
static std::unordered_map<const char*, const save_data_list_t*, cstring_hash, cstring_equal> list_str_hash;
static std::unordered_map<std::tuple<const void*, save_data_tag_t>, const save_data_list_t*, ptr_tag_hash> list_from_ptr_hash;

static bool write_json_std_string(const void* data, bool null_for_empty, Json::Value& output);
static void read_json_std_string(void* data, const Json::Value& json, const char* field);

#include <cassert>

/*
================
G_InitSave
================
*/
void G_InitSave() {
	if (save_data_initialized)
		return;

	for (const save_data_list_t* link = list_head; link; link = link->next) {
		const void* link_ptr = link;

		auto it = list_hash.find(link_ptr);
		if (it != list_hash.end()) {
			const auto& existing = *it;

			assert(false || "invalid save pointer; break here to find which pointer it is"[0]);

			if (!deathmatch->integer) {
				if (g_strict_saves->integer)
					gi.Com_ErrorFmt("link pointer {} already linked as {}; fatal error", link_ptr, existing.second->name);
				else
					gi.Com_PrintFmt("link pointer {} already linked as {}; fatal error", link_ptr, existing.second->name);
			}
		}

		auto sit = list_str_hash.find(link->name);
		if (sit != list_str_hash.end()) {
			const auto& existing = *sit;

			assert(false || "invalid save pointer; break here to find which pointer it is"[0]);

			if (!deathmatch->integer) {
				if (g_strict_saves->integer)
					gi.Com_ErrorFmt("link pointer {} already linked as {}; fatal error", link_ptr, existing.second->name);
				else
					gi.Com_PrintFmt("link pointer {} already linked as {}; fatal error", link_ptr, existing.second->name);
			}
		}

		list_hash.emplace(link_ptr, link);
		list_str_hash.emplace(link->name, link);
		list_from_ptr_hash.emplace(std::make_tuple(link->ptr, link->tag), link);
	}

	save_data_initialized = true;
}

// initializer for save data
save_data_list_t::save_data_list_t(const char* name_in, save_data_tag_t tag_in, const void* ptr_in) :
	name(name_in),
	tag(tag_in),
	ptr(ptr_in) {
	if (save_data_initialized)
		gi.Com_Error("attempted to create save_data_list at runtime");

	next = list_head;
	list_head = this;
}

const save_data_list_t* save_data_list_t::fetch(const void* ptr, save_data_tag_t tag) {
	auto link = list_from_ptr_hash.find(std::make_tuple(ptr, tag));

	if (link != list_from_ptr_hash.end() && link->second->tag == tag)
		return link->second;

	// [0] is just to silence warning
	assert(false || "invalid save pointer; break here to find which pointer it is"[0]);

	if (g_strict_saves->integer)
		gi.Com_ErrorFmt("value pointer {} was not linked to save tag {}\n", ptr, (int32_t)tag);
	else
		gi.Com_PrintFmt("value pointer {} was not linked to save tag {}\n", ptr, (int32_t)tag);

	return nullptr;
}

std::string json_error_stack;

static void json_push_stack(const std::string& stack) {
	json_error_stack += "::" + stack;
}

static void json_pop_stack() {
	size_t o = json_error_stack.find_last_of("::");

	if (o != std::string::npos)
		json_error_stack.resize(o - 1);
}

static void json_print_error(const char* field, const char* message, bool fatal) {
	if (fatal || g_strict_saves->integer)
		gi.Com_ErrorFmt("Error loading JSON\n{}.{}: {}", json_error_stack, field, message);

	gi.Com_PrintFmt("Warning loading JSON\n{}.{}: {}\n", json_error_stack, field, message);
}

using save_void_t = save_data_t<void, UINT_MAX>;

enum class SaveTypeID {
	// never valid
	Invalid,

	// integral types
	Boolean,
	Int8,
	Int16,
	Int32,
	Int64,
	UInt8,
	UInt16,
	UInt32,
	UInt64,
	ENum, // "count" = sizeof(enum_type)

	// floating point
	Float,
	Double,

	String, // "tag" = memory tag, "count" = fixed length if specified, otherwise dynamic

	FixedString, // "count" = length

	FixedArray,
	// for simple types (ones that don't require extra info), "tag" = type and "count" = N
	// otherwise, "type_resolver" for nested arrays, structures, string/function arrays, etc

	Struct, // "count" = sizeof(struct), "structure" = ptr to save_struct_t to save

	BitSet, // bitset; "count" = N bits

	// special Quake types
	Entity,		 // serialized as s.number
	ItemPointer, // serialized as className
	ItemIndex,	 // serialized as className
	Time,		 // serialized as milliseconds
	Data,		 // serialized as name of data ptr from global list; `tag` = list tag
	Inventory,	 // serialized as className => number key/value pairs
	Reinforcements, // serialized as array of data
	SavableDynamic // serialized similar to FixedArray but includes count
};

struct save_struct_t;

struct save_type_t {
	SaveTypeID	id = SaveTypeID::Invalid;
	int			tag = 0;
	size_t		count = 0;
	save_type_t(*type_resolver)() = nullptr;
	const save_struct_t* structure = nullptr;
	bool		never_empty = false;	  // this should be persisted even if all empty
	bool (*is_empty)(const void* data) = nullptr; // override default check

	void (*read)(void* data, const Json::Value& json, const char* field) = nullptr; // for custom reading
	bool (*write)(const void* data, bool null_for_empty, Json::Value& output) = nullptr; // for custom writing
};

struct save_field_t {
	const char* name;
	size_t		offset;
	save_type_t type;

	// for easily chaining with FIELD_AUTO
	constexpr save_field_t& set_is_empty(decltype(save_type_t::is_empty) empty) {
		type.is_empty = empty;
		return *this;
	}
};

struct save_struct_t {
	const char* name;
	const std::initializer_list<save_field_t> fields; // field list

	std::string debug() const {
		std::stringstream s;

		for (auto& field : fields)
			s << field.name << " " << field.offset << " " << static_cast<int>(field.type.id) << " " << static_cast<int>(field.type.tag) << " "
			<< field.type.count << '\n';

		return s.str();
	}
};

// field header macro
#define SAVE_FIELD(n, f) #f, offsetof(n, f)

// save struct header macro
#define INTERNAL_SAVE_STRUCT_START2(struct_name) static const save_struct_t struct_name##_savestruct = { #struct_name, {
#define INTERNAL_SAVE_STRUCT_START1(struct_name) INTERNAL_SAVE_STRUCT_START2(struct_name)
#define SAVE_STRUCT_START INTERNAL_SAVE_STRUCT_START1(DECLARE_SAVE_STRUCT)

#define SAVE_STRUCT_END                                                                                                \
	}                                                                                                                  \
	};

// field header macro for current struct
#define FIELD(f) SAVE_FIELD(DECLARE_SAVE_STRUCT, f)

template<typename T, typename T2 = void>
struct save_type_deducer {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		static_assert(
			!std::is_same_v<T2, void>,
			"Can't automatically deduce type for save; implement save_type_deducer or use an explicit FIELD_ macro.");
		return {};
	}
};

// bool
template<>
struct save_type_deducer<bool> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name, offset, { SaveTypeID::Boolean } };
	}
};

// integral
template<>
struct save_type_deducer<int8_t> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name, offset, { SaveTypeID::Int8 } };
	}
};
template<>
struct save_type_deducer<int16_t> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name, offset, { SaveTypeID::Int16 } };
	}
};
template<>
struct save_type_deducer<int32_t> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name, offset, { SaveTypeID::Int32 } };
	}
};
template<>
struct save_type_deducer<int64_t> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name, offset, { SaveTypeID::Int64 } };
	}
};
template<>
struct save_type_deducer<uint8_t> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name, offset, { SaveTypeID::UInt8 } };
	}
};
template<>
struct save_type_deducer<uint16_t> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name, offset, { SaveTypeID::UInt16 } };
	}
};
template<>
struct save_type_deducer<uint32_t> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name, offset, { SaveTypeID::UInt32 } };
	}
};
template<>
struct save_type_deducer<uint64_t> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name, offset, { SaveTypeID::UInt64 } };
	}
};

// floating point
template<>
struct save_type_deducer<float> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name, offset, { SaveTypeID::Float } };
	}
};
template<>
struct save_type_deducer<double> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name, offset, { SaveTypeID::Double } };
	}
};

// special types
template<>
struct save_type_deducer<gentity_t*> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name, offset, { SaveTypeID::Entity } };
	}
};

template<>
struct save_type_deducer<Item*> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name, offset, { SaveTypeID::ItemPointer } };
	}
};

template<>
struct save_type_deducer<item_id_t> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name, offset, { SaveTypeID::ItemIndex } };
	}
};

template<>
struct save_type_deducer<GameTime> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name, offset, { SaveTypeID::Time } };
	}
};

template<>
struct save_type_deducer<SpawnFlags> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name, offset, { SaveTypeID::UInt32 } };
	}
};

// static strings
template<size_t N>
struct save_type_deducer<char[N]> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name, offset, { SaveTypeID::FixedString, 0, N } };
	}
};

// std::array<char, N> as fixed-length string
template<size_t N>
struct save_type_deducer<std::array<char, N>> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name, offset, { SaveTypeID::FixedString, 0, N } };
	}
};

template<>
struct save_type_deducer<std::string> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name, offset, { SaveTypeID::String, 0, 0, nullptr, nullptr, false, nullptr, read_json_std_string, write_json_std_string } };
	}
};

// enums
template<typename T>
struct save_type_deducer<T, typename std::enable_if_t<std::is_enum_v<T>>> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name,
				 offset,
				 { SaveTypeID::ENum, 0,
				   std::is_same_v<std::underlying_type_t<T>, uint64_t> ? 8
				   : std::is_same_v<std::underlying_type_t<T>, uint32_t> ? 4
				   : std::is_same_v<std::underlying_type_t<T>, uint16_t> ? 2
				   : std::is_same_v<std::underlying_type_t<T>, uint8_t> ? 1
				   : std::is_same_v<std::underlying_type_t<T>, int64_t> ? 8
				   : std::is_same_v<std::underlying_type_t<T>, int32_t> ? 4
				   : std::is_same_v<std::underlying_type_t<T>, int16_t> ? 2
				   : std::is_same_v<std::underlying_type_t<T>, int8_t> ? 1
																		 : 0 } };
	}
};

// vector
template<>
struct save_type_deducer<Vector3> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name, offset, { SaveTypeID::FixedArray, static_cast<int>(SaveTypeID::Float), 3 } };
	}
};

// fixed-size arrays
template<typename T, size_t N>
struct save_type_deducer<T[N], typename std::enable_if_t<!std::is_same_v<T, char>>> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		auto type = save_type_deducer<std::remove_extent_t<T>>::get_save_type(nullptr, 0).type;

		if (type.id <= SaveTypeID::Boolean || type.id >= SaveTypeID::Double)
			return save_field_t{ name, offset, { SaveTypeID::FixedArray, SaveTypeID::Invalid, 0, []() { return save_type_deducer<std::remove_extent_t<T>>::get_save_type(nullptr, 0).type; } } };

		return save_field_t{ name,
				 offset,
				 { SaveTypeID::FixedArray, type.id, N } };
	}
};

// std::array
template<typename T, size_t N>
struct save_type_deducer<std::array<T, N>> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		auto type = save_type_deducer<std::remove_extent_t<T>>::get_save_type(nullptr, 0).type;

		if (type.id <= SaveTypeID::Boolean || type.id >= SaveTypeID::Double)
			return save_field_t{ name, offset, { SaveTypeID::FixedArray, static_cast<int32_t>(SaveTypeID::Invalid), N, []() { return save_type_deducer<std::remove_extent_t<T>>::get_save_type(nullptr, 0).type; } } };

		return save_field_t{ name, offset, { SaveTypeID::FixedArray, static_cast<int32_t>(type.id), N } };
	}
};

// savable_allocated_memory_t
template<typename T, int32_t Tag>
struct save_type_deducer<savable_allocated_memory_t<T, Tag>> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		auto type = save_type_deducer<std::remove_extent_t<T>>::get_save_type(nullptr, 0).type;

		if (type.id <= SaveTypeID::Boolean || type.id >= SaveTypeID::Double)
			return save_field_t{ name, offset, { SaveTypeID::SavableDynamic, static_cast<int32_t>(SaveTypeID::Invalid), Tag, []() { return save_type_deducer<std::remove_extent_t<T>>::get_save_type(nullptr, 0).type; } } };

		return save_field_t{ name, offset, { SaveTypeID::SavableDynamic, static_cast<int32_t>(type.id), Tag } };
	}
};

// save_data_ref<T>
template<typename T, size_t Tag>
struct save_type_deducer<save_data_t<T, Tag>> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name, offset, { SaveTypeID::Data, Tag, 0, nullptr, nullptr, false, nullptr } };
	}
};

// std::bitset<N>
template<size_t N>
struct save_type_deducer<std::bitset<N>> {
	static constexpr save_field_t get_save_type(const char* name, size_t offset) {
		return save_field_t{ name, offset, { SaveTypeID::BitSet, 0, N, nullptr, nullptr, false, nullptr,
				[](void* data, const Json::Value& json, const char* field) {
					std::bitset<N>& as_bitset = *(std::bitset<N> *) data;

					as_bitset.reset();

					if (!json.isString())
						json_print_error(field, "expected string", false);
					else if (strlen(json.asCString()) > N)
						json_print_error(field, "bitset length overflow", false);
					else {
						const char* str = json.asCString();
						size_t len = strlen(str);

						for (size_t i = 0; i < len; i++) {
							if (str[i] == '0')
								continue;
							else if (str[i] == '1')
								as_bitset[i] = true;
							else
								json_print_error(field, "bad bitset value", false);
						}
					}
				},
				[](const void* data, bool null_for_empty, Json::Value& output) -> bool {
					const std::bitset<N>& as_bitset = *(std::bitset<N> *) data;

					if (as_bitset.none()) {
						if (null_for_empty)
							return false;

						output = "";
						return true;
					}

					int32_t num_needed;

					for (num_needed = N - 1; num_needed >= 0; num_needed--)
						if (as_bitset[num_needed])
							break;

					// 00100000, num_needed = 2
					// num_needed always >= 0 since none() check done above
					num_needed++;

					std::string result(num_needed, '0');

					for (size_t n = 0; n < num_needed; n++)
						if (as_bitset[n])
							result[n] = '1';

					output = result;

					return true;
				}
			}
		};
	}
};

// deduce type via template deduction; use this generally
// since it prevents user error and allows seamless type upgrades.
#define FIELD_AUTO(f) save_type_deducer<decltype(DECLARE_SAVE_STRUCT::f)>::get_save_type(FIELD(f))

// simple macro for a `char*` of TAG_LEVEL allocation
#define FIELD_LEVEL_STRING(f)                                                                                          \
	{                                                                                                                  \
		FIELD(f),                                                                                                      \
		{                                                                                                              \
			SaveTypeID::String, TAG_LEVEL                                                                                       \
		}                                                                                                              \
	}

// simple macro for a `char*` of TAG_GAME allocation
#define FIELD_GAME_STRING(f)                                                                                           \
	{                                                                                                                  \
		FIELD(f),                                                                                                      \
		{                                                                                                              \
			SaveTypeID::String, TAG_GAME                                                                                        \
		}                                                                                                              \
	}

// simple macro for a struct type
#define FIELD_STRUCT(f, t)                                                                                             \
	{                                                                                                                  \
		FIELD(f),                                                                                                      \
		{                                                                                                              \
			SaveTypeID::Struct, 0, sizeof(t), nullptr, &t##_savestruct                                                          \
		}                                                                                                              \
	}

// simple macro for a simple field with no parameters
#define FIELD_SIMPLE(f, t)                                                                                             \
	{                                                                                                                  \
		FIELD(f),                                                                                                      \
		{                                                                                                              \
			t                                                                                                          \
		}                                                                                                              \
	}

// macro for creating save type deducer for
// specified struct type
#define MAKE_STRUCT_SAVE_DEDUCER(t)  \
template<> \
struct save_type_deducer<t> \
{ \
	static constexpr save_field_t get_save_type(const char *name, size_t offset) \
	{ \
		return { name, offset, { SaveTypeID::Struct, 0, sizeof(t), nullptr, &t##_savestruct } }; \
	} \
};

#define DECLARE_SAVE_STRUCT LevelEntry
SAVE_STRUCT_START
FIELD_AUTO(mapName),
FIELD_AUTO(longMapName),
FIELD_AUTO(totalSecrets),
FIELD_AUTO(foundSecrets),
FIELD_AUTO(totalMonsters),
FIELD_AUTO(killedMonsters),
FIELD_AUTO(time),
FIELD_AUTO(visit_order)
SAVE_STRUCT_END
#undef DECLARE_SAVE_STRUCT

MAKE_STRUCT_SAVE_DEDUCER(LevelEntry);

// clang-format off
#define DECLARE_SAVE_STRUCT GameLocals
SAVE_STRUCT_START
FIELD_AUTO(help[0].message),
FIELD_AUTO(help[1].message),
FIELD_AUTO(help[0].modificationCount),
FIELD_AUTO(help[1].modificationCount),

// clients is set by load/init only

FIELD_AUTO(spawnPoint),

FIELD_AUTO(maxClients),
FIELD_AUTO(maxEntities),

FIELD_AUTO(crossLevelFlags),
FIELD_AUTO(crossUnitFlags),

FIELD_AUTO(autoSaved),
FIELD_AUTO(levelEntries)
SAVE_STRUCT_END
#undef DECLARE_SAVE_STRUCT

#define DECLARE_SAVE_STRUCT LevelLocals
SAVE_STRUCT_START
FIELD_AUTO(time),

FIELD_AUTO(longName),
FIELD_AUTO(mapName),
FIELD_AUTO(nextMap),

FIELD_AUTO(intermission.time),
FIELD_LEVEL_STRING(changeMap),
FIELD_LEVEL_STRING(achievement),
FIELD_AUTO(intermission.postIntermission),
FIELD_AUTO(intermission.clear),
FIELD_AUTO(intermission.origin),
FIELD_AUTO(intermission.angles),

// picHealth is set by worldspawn
// picPing is set by worldspawn

FIELD_AUTO(campaign.totalSecrets),
FIELD_AUTO(campaign.foundSecrets),

FIELD_AUTO(campaign.totalGoals),
FIELD_AUTO(campaign.foundGoals),

FIELD_AUTO(campaign.totalMonsters),
FIELD_AUTO(campaign.monstersRegistered),
FIELD_AUTO(campaign.killedMonsters),

// currentEntity not necessary to save

FIELD_AUTO(bodyQue),

FIELD_AUTO(powerCubes),

FIELD_AUTO(campaign.disguiseViolator),
FIELD_AUTO(campaign.disguiseViolationTime),

FIELD_AUTO(campaign.coopLevelRestartTime),
FIELD_LEVEL_STRING(campaign.goals),
FIELD_AUTO(campaign.goalNum),
FIELD_AUTO(viewWeaponOffset),

FIELD_AUTO(poi.valid),
FIELD_AUTO(poi.current),
FIELD_AUTO(poi.currentStage),
FIELD_AUTO(poi.currentImage),
FIELD_AUTO(poi.currentDynamic),

FIELD_LEVEL_STRING(start_items),
FIELD_AUTO(no_grapple),
FIELD_AUTO(no_dm_spawnpads),
FIELD_AUTO(no_dm_telepads),
FIELD_AUTO(gravity),
FIELD_AUTO(campaign.hub_map),
FIELD_AUTO(campaign.health_bar_entities),
FIELD_AUTO(intermission.serverFrame),
FIELD_AUTO(campaign.story_active),
FIELD_AUTO(campaign.next_auto_save)
SAVE_STRUCT_END
#undef DECLARE_SAVE_STRUCT

#define DECLARE_SAVE_STRUCT pmove_state_t
SAVE_STRUCT_START
FIELD_AUTO(pmType),
FIELD_AUTO(origin),
FIELD_AUTO(velocity),
FIELD_AUTO(pmFlags),
FIELD_AUTO(pmTime),
FIELD_AUTO(gravity),
FIELD_AUTO(deltaAngles),
FIELD_AUTO(viewHeight)
SAVE_STRUCT_END
#undef DECLARE_SAVE_STRUCT

#define DECLARE_SAVE_STRUCT player_state_t
SAVE_STRUCT_START
FIELD_STRUCT(pmove, pmove_state_t),

FIELD_AUTO(viewAngles),
FIELD_AUTO(viewOffset),
// kickAngles only last 1 frame

FIELD_AUTO(gunAngles),
FIELD_AUTO(gunOffset),
FIELD_AUTO(gunIndex),
FIELD_AUTO(gunFrame),
FIELD_AUTO(gunSkin),
// blend is calculated by ClientEndServerFrame
FIELD_AUTO(fov),

// rdFlags are generated by ClientEndServerFrame
FIELD_AUTO(stats)
SAVE_STRUCT_END
#undef DECLARE_SAVE_STRUCT

#define DECLARE_SAVE_STRUCT height_fog_t
SAVE_STRUCT_START
FIELD_AUTO(start),
FIELD_AUTO(end),
FIELD_AUTO(falloff),
FIELD_AUTO(density)
SAVE_STRUCT_END
#undef DECLARE_SAVE_STRUCT

#define DECLARE_SAVE_STRUCT client_persistant_t
SAVE_STRUCT_START
FIELD_AUTO(userInfo),
FIELD_AUTO(netName),
FIELD_AUTO(hand),

FIELD_AUTO(health),
FIELD_AUTO(maxHealth),
FIELD_AUTO(saved_flags),

FIELD_AUTO(selectedItem),
FIELD_SIMPLE(inventory, SaveTypeID::Inventory),

FIELD_AUTO(ammoMax),

FIELD_AUTO(weapon),
FIELD_AUTO(lastWeapon),

FIELD_AUTO(powerCubes),
FIELD_AUTO(score),

FIELD_AUTO(game_help1changed),
FIELD_AUTO(game_help2changed),
FIELD_AUTO(helpChanged),
FIELD_AUTO(help_time),

// save the wanted fog, but not the current fog
// or transition time so it sends immediately
FIELD_AUTO(wanted_fog),
FIELD_STRUCT(wanted_heightfog, height_fog_t),
FIELD_AUTO(megaTime),
FIELD_AUTO(lives),
FIELD_AUTO(n64_crouch_warn_times),
FIELD_AUTO(n64_crouch_warning)
SAVE_STRUCT_END
#undef DECLARE_SAVE_STRUCT

#define DECLARE_SAVE_STRUCT gclient_t
SAVE_STRUCT_START
FIELD_STRUCT(ps, player_state_t),
// ping... duh

FIELD_STRUCT(pers, client_persistant_t),

FIELD_STRUCT(resp.coopRespawn, client_persistant_t),
FIELD_AUTO(resp.enterTime),
FIELD_AUTO(resp.score),
FIELD_AUTO(resp.cmdAngles),
//FIELD_AUTO(resp.spectator),
// old_pmove is not necessary to persist

// showScores, showInventory, showHelp not necessary

// buttons, oldButtons, latchedButtons not necessary
// weapon.thunk not necessary

FIELD_AUTO(weapon.pending),

// damage_ members are calculated on damage

FIELD_AUTO(killerYaw),

FIELD_AUTO(weaponState),
FIELD_AUTO(kick.angles),
FIELD_AUTO(kick.origin),
FIELD_AUTO(kick.total),
FIELD_AUTO(kick.time),
FIELD_AUTO(feedback.quakeTime),
FIELD_AUTO(feedback.vDamageRoll),
FIELD_AUTO(feedback.vDamagePitch),
FIELD_AUTO(feedback.vDamageTime),
FIELD_AUTO(feedback.fallTime),
FIELD_AUTO(feedback.fallValue),
FIELD_AUTO(feedback.damageAlpha),
FIELD_AUTO(feedback.bonusAlpha),
FIELD_AUTO(feedback.damageBlend),
FIELD_AUTO(vAngle),
FIELD_AUTO(feedback.bobTime),
FIELD_AUTO(oldViewAngles),
FIELD_AUTO(oldVelocity),
FIELD_AUTO(oldGroundEntity),

FIELD_AUTO(nextDrownTime),
FIELD_AUTO(oldWaterLevel),
FIELD_AUTO(breatherSound),

FIELD_AUTO(machinegunShots),

FIELD_AUTO(anim.end),
FIELD_AUTO(anim.priority),
FIELD_AUTO(anim.duck),
FIELD_AUTO(anim.run),

FIELD_AUTO(powerupTimers),
FIELD_AUTO(powerupCounts),

FIELD_AUTO(grenadeBlewUp),
FIELD_AUTO(grenadeTime),
FIELD_AUTO(grenadeFinishedTime),
FIELD_AUTO(weaponSound),

FIELD_AUTO(pickupMessageTime),
FIELD_AUTO(harvesterReminderTime),

// flood stuff is dm only

FIELD_AUTO(respawnMaxTime),

// chasecam not required to persist

FIELD_AUTO(nukeTime),
FIELD_AUTO(trackerPainTime),

// ownedSphere is DM only

FIELD_AUTO(emptyClickSound),
FIELD_AUTO(trail_head),
FIELD_AUTO(trail_tail),

FIELD_GAME_STRING(landmark_name),
FIELD_AUTO(landmark_rel_pos),
FIELD_AUTO(landmark_free_fall),
FIELD_AUTO(landmark_noise_time),
FIELD_AUTO(invisibility_fade_time),
FIELD_AUTO(last_ladder_pos),
FIELD_AUTO(last_ladder_sound),

FIELD_AUTO(sight_entity),
FIELD_AUTO(sight_entity_time),
FIELD_AUTO(sound_entity),
FIELD_AUTO(sound_entity_time),
FIELD_AUTO(sound2_entity),
FIELD_AUTO(sound2_entity_time),

FIELD_AUTO(lastFiringTime),
SAVE_STRUCT_END
#undef DECLARE_SAVE_STRUCT
// clang-format on

static bool edict_t_gravity_is_empty(const void* data) {
	return *((const float*)data) == 1.f;
}

static bool edict_t_gravityVector_is_empty(const void* data) {
	static constexpr Vector3 up_vector = { 0, 0, -1 };
	return *(const Vector3*)data == up_vector;
}

// clang-format off
#define DECLARE_SAVE_STRUCT gentity_t
SAVE_STRUCT_START
// entity_state_t stuff; only one instance
// so no need to do a whole save struct
FIELD_AUTO(s.origin),
FIELD_AUTO(s.angles),
FIELD_AUTO(s.oldOrigin),
FIELD_AUTO(s.modelIndex),
FIELD_AUTO(s.modelIndex2),
FIELD_AUTO(s.modelIndex3),
FIELD_AUTO(s.modelIndex4),
FIELD_AUTO(s.frame),
FIELD_AUTO(s.skinNum),
FIELD_AUTO(s.effects),
FIELD_AUTO(s.renderFX),
// s.solid is set by linkEntity
// events are cleared on each frame, no need to save
FIELD_AUTO(s.sound),
FIELD_AUTO(s.alpha),
FIELD_AUTO(s.scale),
FIELD_AUTO(s.instanceBits),

// server stuff
// client is auto-set
// inUse is implied
FIELD_AUTO(linkCount),
// area, num_clusters, clusternums, headnode, areaNum, areaNum2
// are set by linkEntity and can't be saved

FIELD_AUTO(svFlags),
FIELD_AUTO(mins),
FIELD_AUTO(maxs),
// absMin, absMax and size are set by linkEntity
FIELD_AUTO(solid),
FIELD_AUTO(clipMask),
FIELD_AUTO(owner),

// game stuff
FIELD_AUTO(spawn_count),
FIELD_AUTO(moveType),
FIELD_AUTO(flags),

FIELD_LEVEL_STRING(model),
FIELD_AUTO(freeTime),

FIELD_LEVEL_STRING(message),
FIELD_LEVEL_STRING(className), // FIXME: should allow loading from constants
FIELD_AUTO(spawnFlags),

FIELD_AUTO(timeStamp),

FIELD_AUTO(angle),
FIELD_LEVEL_STRING(target),
FIELD_LEVEL_STRING(targetName),
FIELD_LEVEL_STRING(killTarget),
FIELD_LEVEL_STRING(team),
FIELD_LEVEL_STRING(pathTarget),
FIELD_LEVEL_STRING(deathTarget),
FIELD_LEVEL_STRING(healthTarget),
FIELD_LEVEL_STRING(itemTarget),
FIELD_LEVEL_STRING(combatTarget),
FIELD_AUTO(targetEnt),

FIELD_AUTO(speed),
FIELD_AUTO(accel),
FIELD_AUTO(decel),
FIELD_AUTO(moveDir),
FIELD_AUTO(pos1),
FIELD_AUTO(pos2),
FIELD_AUTO(pos3),

FIELD_AUTO(velocity),
FIELD_AUTO(aVelocity),
FIELD_AUTO(mass),
FIELD_AUTO(airFinished),
FIELD_AUTO(gravity).set_is_empty(edict_t_gravity_is_empty),

FIELD_AUTO(goalEntity),
FIELD_AUTO(moveTarget),
FIELD_AUTO(yawSpeed),
FIELD_AUTO(ideal_yaw),

FIELD_AUTO(nextThink),
FIELD_AUTO(preThink),
FIELD_AUTO(postThink),
FIELD_AUTO(think),
FIELD_AUTO(touch),
FIELD_AUTO(use),
FIELD_AUTO(pain),
FIELD_AUTO(die),

FIELD_AUTO(touch_debounce_time),
FIELD_AUTO(pain_debounce_time),
FIELD_AUTO(damage_debounce_time),
FIELD_AUTO(fly_sound_debounce_time),
FIELD_AUTO(last_move_time),

FIELD_AUTO(health),
FIELD_AUTO(maxHealth),
FIELD_AUTO(gibHealth),
FIELD_AUTO(deadFlag),
FIELD_AUTO(show_hostile),

FIELD_AUTO(powerArmorTime),

FIELD_AUTO(map),

FIELD_AUTO(viewHeight),
FIELD_AUTO(takeDamage),
FIELD_AUTO(dmg),
FIELD_AUTO(splashDamage),
FIELD_AUTO(splashRadius),
FIELD_AUTO(sounds),
FIELD_AUTO(count),

FIELD_AUTO(chain),
FIELD_AUTO(enemy),
FIELD_AUTO(oldEnemy),
FIELD_AUTO(activator),
FIELD_AUTO(groundEntity),
FIELD_AUTO(groundEntity_linkCount),
FIELD_AUTO(teamChain),
FIELD_AUTO(teamMaster),

FIELD_AUTO(myNoise),
FIELD_AUTO(myNoise2),

FIELD_AUTO(noiseIndex),
FIELD_AUTO(noiseIndex2),
FIELD_AUTO(volume),
FIELD_AUTO(attenuation),

FIELD_AUTO(wait),
FIELD_AUTO(delay),
FIELD_AUTO(random),

FIELD_AUTO(teleportTime),

FIELD_AUTO(waterType),
FIELD_AUTO(waterLevel),

FIELD_AUTO(moveOrigin),
FIELD_AUTO(moveAngles),

FIELD_AUTO(style),
FIELD_LEVEL_STRING(style_on),
FIELD_LEVEL_STRING(style_off),

FIELD_AUTO(item),
FIELD_AUTO(crosslevel_flags),

// MoveInfo
FIELD_AUTO(moveInfo.startOrigin),
FIELD_AUTO(moveInfo.startAngles),
FIELD_AUTO(moveInfo.endOrigin),
FIELD_AUTO(moveInfo.endAngles),
FIELD_AUTO(moveInfo.endAnglesReversed),

FIELD_AUTO(moveInfo.sound_start),
FIELD_AUTO(moveInfo.sound_middle),
FIELD_AUTO(moveInfo.sound_end),

FIELD_AUTO(moveInfo.accel),
FIELD_AUTO(moveInfo.speed),
FIELD_AUTO(moveInfo.decel),
FIELD_AUTO(moveInfo.distance),

FIELD_AUTO(moveInfo.wait),

FIELD_AUTO(moveInfo.state),
FIELD_AUTO(moveInfo.reversing),
FIELD_AUTO(moveInfo.dir),
FIELD_AUTO(moveInfo.dest),
FIELD_AUTO(moveInfo.currentSpeed),
FIELD_AUTO(moveInfo.moveSpeed),
FIELD_AUTO(moveInfo.nextSpeed),
FIELD_AUTO(moveInfo.remainingDistance),
FIELD_AUTO(moveInfo.decelDistance),
FIELD_AUTO(moveInfo.endFunc),
FIELD_AUTO(moveInfo.blocked),

FIELD_AUTO(moveInfo.curveRef),
FIELD_AUTO(moveInfo.curvePositions),
FIELD_AUTO(moveInfo.curveFrame),
FIELD_AUTO(moveInfo.subFrame),
FIELD_AUTO(moveInfo.numSubFrames),
FIELD_AUTO(moveInfo.numFramesDone),

// MonsterInfo
FIELD_AUTO(monsterInfo.active_move),
FIELD_AUTO(monsterInfo.next_move),
FIELD_AUTO(monsterInfo.aiFlags),
FIELD_AUTO(monsterInfo.nextFrame),
FIELD_AUTO(monsterInfo.scale),

FIELD_AUTO(monsterInfo.stand),
FIELD_AUTO(monsterInfo.idle),
FIELD_AUTO(monsterInfo.search),
FIELD_AUTO(monsterInfo.walk),
FIELD_AUTO(monsterInfo.run),
FIELD_AUTO(monsterInfo.dodge),
FIELD_AUTO(monsterInfo.attack),
FIELD_AUTO(monsterInfo.melee),
FIELD_AUTO(monsterInfo.sight),
FIELD_AUTO(monsterInfo.checkAttack),
FIELD_AUTO(monsterInfo.setSkin),

FIELD_AUTO(monsterInfo.pauseTime),
FIELD_AUTO(monsterInfo.attackFinished),
FIELD_AUTO(monsterInfo.fireWait),

FIELD_AUTO(monsterInfo.savedGoal),
FIELD_AUTO(monsterInfo.searchTime),
FIELD_AUTO(monsterInfo.trailTime),
FIELD_AUTO(monsterInfo.lastSighting),
FIELD_AUTO(monsterInfo.attackState),
FIELD_AUTO(monsterInfo.lefty),
FIELD_AUTO(monsterInfo.idleTime),
FIELD_AUTO(monsterInfo.linkCount),

FIELD_AUTO(monsterInfo.powerArmorType),
FIELD_AUTO(monsterInfo.powerArmorPower),
FIELD_AUTO(monsterInfo.initialPowerArmorType),
FIELD_AUTO(monsterInfo.max_power_armor_power),
FIELD_AUTO(monsterInfo.weaponSound),
FIELD_AUTO(monsterInfo.engineSound),

FIELD_AUTO(monsterInfo.blocked),
FIELD_AUTO(monsterInfo.last_hint_time),
FIELD_AUTO(monsterInfo.goal_hint),
FIELD_AUTO(monsterInfo.medicTries),
FIELD_AUTO(monsterInfo.badMedic1),
FIELD_AUTO(monsterInfo.badMedic2),
FIELD_AUTO(monsterInfo.healer),
FIELD_AUTO(monsterInfo.duck),
FIELD_AUTO(monsterInfo.unDuck),
FIELD_AUTO(monsterInfo.sideStep),
FIELD_AUTO(monsterInfo.base_height),
FIELD_AUTO(monsterInfo.next_duck_time),
FIELD_AUTO(monsterInfo.duck_wait_time),
FIELD_AUTO(monsterInfo.last_player_enemy),
FIELD_AUTO(monsterInfo.blindFire),
FIELD_AUTO(monsterInfo.canJump),
FIELD_AUTO(monsterInfo.had_visibility),
FIELD_AUTO(monsterInfo.dropHeight),
FIELD_AUTO(monsterInfo.jumpHeight),
FIELD_AUTO(monsterInfo.blind_fire_delay),
FIELD_AUTO(monsterInfo.blind_fire_target),
FIELD_AUTO(monsterInfo.teleportReturnOrigin),
FIELD_AUTO(monsterInfo.teleportReturnTime),
FIELD_AUTO(monsterInfo.teleportActive),
FIELD_AUTO(monsterInfo.monster_slots),
FIELD_AUTO(monsterInfo.monster_used),
FIELD_AUTO(monsterInfo.commander),
FIELD_AUTO(monsterInfo.quad_time),
FIELD_AUTO(monsterInfo.invincibility_time),
FIELD_AUTO(monsterInfo.double_time),

FIELD_AUTO(monsterInfo.surprise_time),
FIELD_AUTO(monsterInfo.armorType),
FIELD_AUTO(monsterInfo.armor_power),
FIELD_AUTO(monsterInfo.close_sight_tripped),
FIELD_AUTO(monsterInfo.melee_debounce_time),
FIELD_AUTO(monsterInfo.strafe_check_time),
FIELD_AUTO(monsterInfo.base_health),
FIELD_AUTO(monsterInfo.health_scaling),
FIELD_AUTO(monsterInfo.next_move_time),
FIELD_AUTO(monsterInfo.bad_move_time),
FIELD_AUTO(monsterInfo.bump_time),
FIELD_AUTO(monsterInfo.random_change_time),
FIELD_AUTO(monsterInfo.path_blocked_counter),
FIELD_AUTO(monsterInfo.path_wait_time),
FIELD_AUTO(monsterInfo.combatStyle),

FIELD_AUTO(monsterInfo.fly_max_distance),
FIELD_AUTO(monsterInfo.fly_min_distance),
FIELD_AUTO(monsterInfo.fly_acceleration),
FIELD_AUTO(monsterInfo.fly_speed),
FIELD_AUTO(monsterInfo.fly_ideal_position),
FIELD_AUTO(monsterInfo.fly_position_time),
FIELD_AUTO(monsterInfo.fly_buzzard),
FIELD_AUTO(monsterInfo.fly_above),
FIELD_AUTO(monsterInfo.fly_pinned),
FIELD_AUTO(monsterInfo.fly_thrusters),
FIELD_AUTO(monsterInfo.fly_recovery_time),
FIELD_AUTO(monsterInfo.fly_recovery_dir),

FIELD_AUTO(monsterInfo.teleport_saved_origin),
FIELD_AUTO(monsterInfo.teleport_return_time),
FIELD_AUTO(monsterInfo.teleport_active),

FIELD_AUTO(monsterInfo.checkattack_time),
FIELD_AUTO(monsterInfo.startFrame),
FIELD_AUTO(monsterInfo.dodge_time),
FIELD_AUTO(monsterInfo.move_block_counter),
FIELD_AUTO(monsterInfo.move_block_change_time),
FIELD_AUTO(monsterInfo.react_to_damage_time),
FIELD_AUTO(monsterInfo.jump_time),

FIELD_SIMPLE(monsterInfo.reinforcements, SaveTypeID::Reinforcements),
FIELD_AUTO(monsterInfo.chosen_reinforcements),

FIELD_AUTO(monsterInfo.physicsChange),

// back to gentity_t
FIELD_AUTO(plat2flags),
FIELD_AUTO(offset),
FIELD_AUTO(gravityVector).set_is_empty(edict_t_gravityVector_is_empty),
FIELD_AUTO(bad_area),
FIELD_AUTO(hint_chain),
FIELD_AUTO(monster_hint_chain),
FIELD_AUTO(target_hint_chain),
FIELD_AUTO(hint_chain_id),

FIELD_AUTO(clock_message),
FIELD_AUTO(dead_time),
FIELD_AUTO(beam),
FIELD_AUTO(beam2),
FIELD_AUTO(proboscus),
FIELD_AUTO(disintegrator),
FIELD_AUTO(disintegrator_time),
FIELD_AUTO(hackFlags),

FIELD_AUTO(fog.color),
FIELD_AUTO(fog.density),
FIELD_AUTO(fog.color_off),
FIELD_AUTO(fog.density_off),
FIELD_AUTO(fog.sky_factor),
FIELD_AUTO(fog.sky_factor_off),

FIELD_AUTO(heightfog.falloff),
FIELD_AUTO(heightfog.density),
FIELD_AUTO(heightfog.start_color),
FIELD_AUTO(heightfog.start_dist),
FIELD_AUTO(heightfog.end_color),
FIELD_AUTO(heightfog.end_dist),

FIELD_AUTO(heightfog.falloff_off),
FIELD_AUTO(heightfog.density_off),
FIELD_AUTO(heightfog.start_color_off),
FIELD_AUTO(heightfog.start_dist_off),
FIELD_AUTO(heightfog.end_color_off),
FIELD_AUTO(heightfog.end_dist_off),

FIELD_AUTO(itemPickedUpBy),
FIELD_AUTO(slime_debounce_time),

FIELD_AUTO(bmodel_anim.start),
FIELD_AUTO(bmodel_anim.end),
FIELD_AUTO(bmodel_anim.style),
FIELD_AUTO(bmodel_anim.speed),
FIELD_AUTO(bmodel_anim.nowrap),

FIELD_AUTO(bmodel_anim.alt_start),
FIELD_AUTO(bmodel_anim.alt_end),
FIELD_AUTO(bmodel_anim.alt_style),
FIELD_AUTO(bmodel_anim.alt_speed),
FIELD_AUTO(bmodel_anim.alt_nowrap),

FIELD_AUTO(bmodel_anim.enabled),
FIELD_AUTO(bmodel_anim.alternate),
FIELD_AUTO(bmodel_anim.currently_alternate),
FIELD_AUTO(bmodel_anim.next_tick),

FIELD_AUTO(lastMOD.id),
FIELD_AUTO(lastMOD.friendly_fire),

SAVE_STRUCT_END
#undef DECLARE_SAVE_STRUCT
// clang-format on

inline size_t get_simple_type_size(SaveTypeID id, bool fatal = true) {
	switch (id) {
	case SaveTypeID::Boolean:
		return sizeof(bool);
	case SaveTypeID::Int8:
	case SaveTypeID::UInt8:
		return sizeof(uint8_t);
	case SaveTypeID::Int16:
	case SaveTypeID::UInt16:
		return sizeof(uint16_t);
	case SaveTypeID::Int32:
	case SaveTypeID::UInt32:
		return sizeof(uint32_t);
	case SaveTypeID::Int64:
	case SaveTypeID::UInt64:
	case SaveTypeID::Time:
		return sizeof(uint64_t);
	case SaveTypeID::Float:
		return sizeof(float);
	case SaveTypeID::Double:
		return sizeof(double);
	case SaveTypeID::Entity:
	case SaveTypeID::ItemPointer:
		return sizeof(size_t);
	case SaveTypeID::ItemIndex:
		return sizeof(uint32_t);
	case SaveTypeID::SavableDynamic:
		return sizeof(savable_allocated_memory_t<void*, 0>);
	default:
		if (fatal)
			gi.Com_ErrorFmt("Can't calculate static size for type ID {}", (int32_t)id);
		break;
	}

	return 0;
}

static size_t get_complex_type_size(const save_type_t& type) {
	// these are simple types
	if (auto simple = get_simple_type_size(type.id, false))
		return simple;

	switch (type.id) {
	case SaveTypeID::Struct:
		return type.count;
	case SaveTypeID::FixedArray: {
		save_type_t element_type;
		size_t element_size;

		if (type.type_resolver) {
			element_type = type.type_resolver();
			element_size = get_complex_type_size(element_type);
		}
		else {
			element_size = get_simple_type_size((SaveTypeID)type.tag);
			element_type = { (SaveTypeID)type.tag };
		}

		return element_size * type.count;
	}
	default:
		gi.Com_ErrorFmt("Can't calculate static size for type ID {}", (int32_t)type.id);
	}

	return 0;
}

void read_save_struct_json(const Json::Value& json, void* data, const save_struct_t* structure);

static void read_save_type_json(const Json::Value& json, void* data, const save_type_t* type, const char* field) {
	if (type->read) {
		type->read(data, json, field);
		return;
	}

	switch (type->id) {
		using enum SaveTypeID;
	case Boolean:
		if (!json.isBool())
			json_print_error(field, "expected boolean", false);
		else
			*((bool*)data) = json.asBool();
		return;
	case ENum:
		if (!json.isIntegral())
			json_print_error(field, "expected integer", false);
		else if (type->count == 1) {
			if (json.isInt()) {
				if (json.asInt() < INT8_MIN || json.asInt() > INT8_MAX)
					json_print_error(field, "int8 out of range", false);
				else
					*((int8_t*)data) = json.asInt();
			}
			else if (json.isUInt()) {
				if (json.asUInt() > UINT8_MAX)
					json_print_error(field, "uint8 out of range", false);
				else
					*((uint8_t*)data) = json.asUInt();
			}
			else
				json_print_error(field, "int8 out of range (is 64-bit)", false);
			return;
		}
		else if (type->count == 2) {
			if (json.isInt()) {
				if (json.asInt() < INT16_MIN || json.asInt() > INT16_MAX)
					json_print_error(field, "int16 out of range", false);
				else
					*((int16_t*)data) = json.asInt();
			}
			else if (json.isUInt()) {
				if (json.asUInt() > UINT16_MAX)
					json_print_error(field, "uint16 out of range", false);
				else
					*((uint16_t*)data) = json.asUInt();
			}
			else
				json_print_error(field, "int16 out of range (is 64-bit)", false);
			return;
		}
		else if (type->count == 4) {
			if (json.isInt()) {
				if (json.asInt64() < INT32_MIN || json.asInt64() > INT32_MAX)
					json_print_error(field, "int32 out of range", false);
				else
					*((int32_t*)data) = json.asInt();
			}
			else if (json.isUInt()) {
				if (json.asUInt64() > UINT32_MAX)
					json_print_error(field, "uint32 out of range", false);
				else
					*((uint32_t*)data) = json.asUInt();
			}
			else
				json_print_error(field, "int32 out of range (is 64-bit)", false);
			return;
		}
		else if (type->count == 8) {
			if (json.isInt64())
				*((int64_t*)data) = json.asInt64();
			else if (json.isUInt64())
				*((int64_t*)data) = json.asUInt64();
			else if (json.isInt())
				*((int64_t*)data) = json.asInt();
			else if (json.isUInt())
				*((int64_t*)data) = json.asUInt();
			else
				json_print_error(field, "int64 not integral", false);
			return;
		}

		json_print_error(field, "invalid enum size", true);
		return;
	case Int8:
		if (!json.isInt())
			json_print_error(field, "expected integer", false);
		else if (json.asInt() < INT8_MIN || json.asInt() > INT8_MAX)
			json_print_error(field, "int8 out of range", false);
		else
			*((int8_t*)data) = json.asInt();
		return;
	case Int16:
		if (!json.isInt())
			json_print_error(field, "expected integer", false);
		else if (json.asInt() < INT16_MIN || json.asInt() > INT16_MAX)
			json_print_error(field, "int16 out of range", false);
		else
			*((int16_t*)data) = json.asInt();
		return;
	case Int32:
		if (!json.isInt())
			json_print_error(field, "expected integer", false);
		else if (json.asInt() < INT32_MIN || json.asInt() > INT32_MAX)
			json_print_error(field, "int32 out of range", false);
		else
			*((int32_t*)data) = json.asInt();
		return;
	case Int64:
		if (!json.isInt64())
			json_print_error(field, "expected integer", false);
		else
			*((int64_t*)data) = json.asInt64();
		return;
	case UInt8:
		if (!json.isUInt())
			json_print_error(field, "expected integer", false);
		else if (json.asUInt() > UINT8_MAX)
			json_print_error(field, "uint8 out of range", false);
		else
			*((uint8_t*)data) = json.asUInt();
		return;
	case UInt16:
		if (!json.isUInt())
			json_print_error(field, "expected integer", false);
		else if (json.asUInt() > UINT16_MAX)
			json_print_error(field, "uint16 out of range", false);
		else
			*((uint16_t*)data) = json.asUInt();
		return;
	case UInt32:
		if (!json.isUInt())
			json_print_error(field, "expected integer", false);
		else if (json.asUInt() > UINT32_MAX)
			json_print_error(field, "uint32 out of range", false);
		else
			*((uint32_t*)data) = json.asUInt();
		return;
	case UInt64:
		if (!json.isUInt64())
			json_print_error(field, "expected integer", false);
		else
			*((uint64_t*)data) = json.asUInt64();
		return;
	case Float:
		if (!json.isDouble())
			json_print_error(field, "expected number", false);
		else if (isnan(json.asDouble()))
			*((float*)data) = std::numeric_limits<float>::quiet_NaN();
		else
			*((float*)data) = json.asFloat();
		return;
	case Double:
		if (!json.isDouble())
			json_print_error(field, "expected number", false);
		else
			*((double*)data) = json.asDouble();
		return;
	case String:
		if (json.isNull())
			*((char**)data) = nullptr;
		else if (json.isString()) {
			if (type->count && strlen(json.asCString()) >= type->count)
				json_print_error(field, "static-length dynamic string overrun", false);
			else {
				size_t len = strlen(json.asCString());
				char* str = *((char**)data) = (char*)gi.TagMalloc(type->count ? type->count : (len + 1), static_cast<int>(type->tag));
				strcpy(str, json.asCString());
				str[len] = 0;
			}
		}
		else if (json.isArray()) {
			if (type->count && json.size() >= type->count - 1)
				json_print_error(field, "static-length dynamic string overrun", false);
			else {
				size_t len = json.size();
				char* str = *((char**)data) = (char*)gi.TagMalloc(type->count ? type->count : (len + 1), static_cast<int>(type->tag));

				for (Json::Value::ArrayIndex i = 0; i < json.size(); i++) {
					const Json::Value& chr = json[i];

					if (!chr.isInt())
						json_print_error(field, "expected number", false);
					else if (chr.asInt() < 0 || chr.asInt() > UINT8_MAX)
						json_print_error(field, "char out of range", false);

					str[i] = chr.asInt();
				}

				str[len] = 0;
			}
		}
		else
			json_print_error(field, "expected string, array or null", false);
		return;
	case FixedString:
		if (json.isString()) {
			if (type->count && strlen(json.asCString()) >= type->count)
				json_print_error(field, "fixed length string overrun", false);
			else
				strcpy((char*)data, json.asCString());
		}
		else if (json.isArray()) {
			if (type->count && json.size() >= type->count - 1)
				json_print_error(field, "fixed length string overrun", false);
			else {
				Json::Value::ArrayIndex i;

				for (i = 0; i < json.size(); i++) {
					const Json::Value& chr = json[i];

					if (!chr.isInt())
						json_print_error(field, "expected number", false);
					else if (chr.asInt() < 0 || chr.asInt() > UINT8_MAX)
						json_print_error(field, "char out of range", false);

					((char*)data)[i] = chr.asInt();
				}

				((char*)data)[i] = 0;
			}
		}
		else
			json_print_error(field, "expected string or array", false);
		return;
	case FixedArray:
		if (!json.isArray())
			json_print_error(field, "expected array", false);
		else if (type->count != json.size())
			json_print_error(field, "fixed array length mismatch", false);
		else {
			uint8_t* element = (uint8_t*)data;
			size_t			  element_size;
			save_type_t       element_type;

			if (type->type_resolver) {
				element_type = type->type_resolver();
				element_size = get_complex_type_size(element_type);
			}
			else {
				element_size = get_simple_type_size((SaveTypeID)type->tag);
				element_type = { (SaveTypeID)type->tag };
			}

			for (Json::Value::ArrayIndex i = 0; i < type->count; i++, element += element_size) {
				const Json::Value& v = json[i];
				read_save_type_json(v, element, &element_type,
					fmt::format("[{}]", i).c_str());
			}
		}

		return;
	case SavableDynamic:
		if (!json.isArray())
			json_print_error(field, "expected array", false);
		else {
			savable_allocated_memory_t<void, 0>* savptr = (savable_allocated_memory_t<void, 0> *) data;
			size_t			  element_size;
			save_type_t       element_type;

			if (type->type_resolver) {
				element_type = type->type_resolver();
				element_size = get_complex_type_size(element_type);
			}
			else {
				element_size = get_simple_type_size((SaveTypeID)type->tag);
				element_type = { (SaveTypeID)type->tag };
			}

			savptr->count = json.size();
			savptr->ptr = gi.TagMalloc(element_size * savptr->count, type->count);

			byte* out_element = (byte*)savptr->ptr;

			for (Json::Value::ArrayIndex i = 0; i < savptr->count; i++, out_element += element_size) {
				const Json::Value& v = json[i];
				read_save_type_json(v, out_element, &element_type,
					fmt::format("[{}]", i).c_str());
			}
		}

		return;
	case BitSet:
		type->read(data, json, field);
		return;
	case Struct:
		if (!json.isNull()) {
			json_push_stack(field);
			read_save_struct_json(json, data, type->structure);
			json_pop_stack();
		}
		return;
	case Entity:
		if (json.isNull())
			*((gentity_t**)data) = nullptr;
		else if (!json.isUInt())
			json_print_error(field, "expected null or integer", false);
		else if (json.asUInt() >= globals.maxEntities)
			json_print_error(field, "entity index out of range", false);
		else
			*((gentity_t**)data) = globals.gentities + json.asUInt();

		return;
	case ItemPointer:
	case ItemIndex: {
		Item* item;

		if (json.isNull())
			item = nullptr;
		else if (json.isString()) {
			const char* className = json.asCString();
			item = FindItemByClassname(className);

			if (item == nullptr) {
				json_print_error(field, G_Fmt("item {} missing", className).data(), false);
				return;
			}
		}
		else {
			json_print_error(field, "expected null or string", false);
			return;
		}

		if (type->id == ItemPointer)
			*((Item**)data) = item;
		else
			*((int32_t*)data) = item ? item->id : 0;
		return;
	}
	case Time:
		if (!json.isInt64())
			json_print_error(field, "expected integer", false);
		else
			*((GameTime*)data) = GameTime::from_ms(json.asInt64());
		return;
	case Data:
		if (json.isNull())
			*((void**)data) = nullptr;
		else if (!json.isString())
			json_print_error(field, "expected null or string", false);
		else {
			const char* name = json.asCString();
			auto link = list_str_hash.find(name);

			if (link == list_str_hash.end()) {
				json_print_error(
					field, G_Fmt("unknown pointer {} in list {}", name, type->tag).data(), false);
				(*reinterpret_cast<save_void_t*>(data)) = nullptr;
			}
			else
				(*reinterpret_cast<save_void_t*>(data)) = save_void_t(link->second);
		}
		return;
	case Inventory:
		if (!json.isObject())
			json_print_error(field, "expected object", false);
		else {
			int32_t* inventory_ptr = (int32_t*)data;

			//for (auto key : json.getMemberNames())
			for (auto it = json.begin(); it != json.end(); it++) {
				//const char		   *className = key.c_str();
				const char* dummy;
				const char* className = it.memberName(&dummy);
				const Json::Value& value = *it;

				if (!value.isInt()) {
					json_push_stack(className);
					json_print_error(field, "expected integer", false);
					json_pop_stack();
					continue;
				}

				Item* item = FindItemByClassname(className);

				if (!item) {
					json_push_stack(className);
					json_print_error(field, G_Fmt("can't find item {}", className).data(), false);
					json_pop_stack();
					continue;
				}

				inventory_ptr[item->id] = value.asInt();
			}
			return;
		}
		return;
	case Reinforcements:
		if (!json.isArray() && !json.isObject())
			json_print_error(field, "expected array or object", false);
		else {
			reinforcement_list_t* list_ptr = (reinforcement_list_t*)data;
			const Json::Value* entries = &json;

			list_ptr->next_reinforcement = 0;
			list_ptr->spawn_counts = nullptr;

			if (json.isObject()) {
				if (json["next"].isUInt())
					list_ptr->next_reinforcement = json["next"].asUInt();

				entries = &json["entries"];
			}

			if (!entries->isArray()) {
				json_print_error(field, "expected array", false);
				return;
			}

			list_ptr->num_reinforcements = entries->size();
			list_ptr->reinforcements = (reinforcement_t*)gi.TagMalloc(sizeof(reinforcement_t) * list_ptr->num_reinforcements, TAG_LEVEL);
			list_ptr->spawn_counts = (uint32_t*)gi.TagMalloc(sizeof(uint32_t) * list_ptr->num_reinforcements, TAG_LEVEL);
			memset(list_ptr->spawn_counts, 0, sizeof(uint32_t) * list_ptr->num_reinforcements);

			reinforcement_t* p = list_ptr->reinforcements;

			for (Json::Value::ArrayIndex i = 0; i < entries->size(); i++, p++) {
				const Json::Value& value = (*entries)[i];

				if (!value.isObject()) {
					json_push_stack(fmt::format("{}", i));
					json_print_error(field, "expected object", false);
					json_pop_stack();
					continue;
				}

				// quick type checks

				if (!value["classname"].isString()) {
					json_push_stack(fmt::format("{}.className", i));
					json_print_error(field, "expected string", false);
					json_pop_stack();
					continue;
				}

				if (!value["mins"].isArray() || value["mins"].size() != 3) {
					json_push_stack(fmt::format("{}.mins", i));
					json_print_error(field, "expected array[3]", false);
					json_pop_stack();
					continue;
				}

				if (!value["maxs"].isArray() || value["maxs"].size() != 3) {
					json_push_stack(fmt::format("{}.maxs", i));
					json_print_error(field, "expected array[3]", false);
					json_pop_stack();
					continue;
				}

				if (!value["strength"].isInt()) {
					json_push_stack(fmt::format("{}.strength", i));
					json_print_error(field, "expected int", false);
					json_pop_stack();
					continue;
				}

				if (value.isMember("count")) {
					if (value["count"].isUInt())
						list_ptr->spawn_counts[i] = value["count"].asUInt();
					else
						json_print_error(field, "expected unsigned count", false);
				}

				p->className = CopyString(value["classname"].asCString(), TAG_LEVEL);
				p->strength = value["strength"].asInt();

				for (int32_t x = 0; x < 3; x++) {
					p->mins[x] = value["mins"][x].asInt();
					p->maxs[x] = value["maxs"][x].asInt();
				}
			}

			if (list_ptr->num_reinforcements && list_ptr->next_reinforcement >= list_ptr->num_reinforcements)
				list_ptr->next_reinforcement %= list_ptr->num_reinforcements;
		}
		return;
	default:
		gi.Com_ErrorFmt("Can't read type ID {}", (int32_t)type->id);
		break;
	}
}

bool write_save_struct_json(const void* data, const save_struct_t* structure, bool null_for_empty, Json::Value& output);

#define TYPED_DATA_IS_EMPTY(type, expr) (type->is_empty ? type->is_empty(data) : (expr))

static inline bool string_is_high(const char* c) {
	for (size_t i = 0; i < strlen(c); i++)
		if (c[i] & 128)
			return true;

	return false;
}

static inline Json::Value string_to_bytes(const char* c) {
	Json::Value array(Json::arrayValue);

	for (size_t i = 0; i < strlen(c); i++)
		array.append((int32_t)(unsigned char)c[i]);

	return array;
}

static bool write_json_std_string(const void* data, bool null_for_empty, Json::Value& output) {
	const std::string& str = *reinterpret_cast<const std::string*>(data);

	if (null_for_empty && str.empty())
		return false;

	if (string_is_high(str.c_str()))
		output = string_to_bytes(str.c_str());
	else
		output = Json::Value(str);

	return true;
}

static void read_json_std_string(void* data, const Json::Value& json, const char* field) {
	std::string& str = *reinterpret_cast<std::string*>(data);

	if (json.isNull()) {
		str.clear();
	}
	else if (json.isString()) {
		str = json.asString();
	}
	else if (json.isArray()) {
		std::string result;
		result.reserve(json.size());

		for (const Json::Value& chr : json) {
			if (!chr.isInt()) {
				json_print_error(field, "expected number", false);
				continue;
			}

			int value = chr.asInt();
			if (value < 0 || value > UINT8_MAX) {
				json_print_error(field, "char out of range", false);
				continue;
			}

			result.push_back(static_cast<char>(value & 0xFF));
		}

		str = std::move(result);
	}
	else {
		json_print_error(field, "expected string, array or null", false);
		str.clear();
	}
}

// fetch a JSON value for the specified data.
// if allow_empty is true, false will be returned for
// values that are the same as zero'd memory, to save
// space in the resulting JSON. output will be
// unmodified in that case.
static bool write_save_type_json(const void* data, const save_type_t* type, bool null_for_empty, Json::Value& output) {
	if (type->write)
		return type->write(data, null_for_empty, output);

	switch (type->id) {
	case SaveTypeID::Boolean:
		if (null_for_empty && TYPED_DATA_IS_EMPTY(type, !*(const bool*)data))
			return false;

		output = Json::Value(*(const bool*)data);
		return true;
	case SaveTypeID::ENum:
		if (type->count == 1) {
			if (null_for_empty && TYPED_DATA_IS_EMPTY(type, !*(const int8_t*)data))
				return false;

			output = Json::Value(*(const int8_t*)data);
			return true;
		}
		else if (type->count == 2) {
			if (null_for_empty && TYPED_DATA_IS_EMPTY(type, !*(const int16_t*)data))
				return false;

			output = Json::Value(*(const int16_t*)data);
			return true;
		}
		else if (type->count == 4) {
			if (null_for_empty && TYPED_DATA_IS_EMPTY(type, !*(const int32_t*)data))
				return false;

			output = Json::Value(*(const int32_t*)data);
			return true;
		}
		else if (type->count == 8) {
			if (null_for_empty && TYPED_DATA_IS_EMPTY(type, !*(const int64_t*)data))
				return false;

			output = Json::Value(*(const int64_t*)data);
			return true;
		}
		gi.Com_Error("invalid enum length");
		break;
	case SaveTypeID::Int8:
		if (null_for_empty && TYPED_DATA_IS_EMPTY(type, !*(const int8_t*)data))
			return false;

		output = Json::Value(*(const int8_t*)data);
		return true;
	case SaveTypeID::Int16:
		if (null_for_empty && TYPED_DATA_IS_EMPTY(type, !*(const int16_t*)data))
			return false;

		output = Json::Value(*(const int16_t*)data);
		return true;
	case SaveTypeID::Int32:
		if (null_for_empty && TYPED_DATA_IS_EMPTY(type, !*(const int32_t*)data))
			return false;

		output = Json::Value(*(const int32_t*)data);
		return true;
	case SaveTypeID::Int64:
		if (null_for_empty && TYPED_DATA_IS_EMPTY(type, !*(const int64_t*)data))
			return false;

		output = Json::Value(*(const int64_t*)data);
		return true;
	case SaveTypeID::UInt8:
		if (null_for_empty && TYPED_DATA_IS_EMPTY(type, !*(const uint8_t*)data))
			return false;

		output = Json::Value(*(const uint8_t*)data);
		return true;
	case SaveTypeID::UInt16:
		if (null_for_empty && TYPED_DATA_IS_EMPTY(type, !*(const uint16_t*)data))
			return false;

		output = Json::Value(*(const uint16_t*)data);
		return true;
	case SaveTypeID::UInt32:
		if (null_for_empty && TYPED_DATA_IS_EMPTY(type, !*(const uint32_t*)data))
			return false;

		output = Json::Value(*(const uint32_t*)data);
		return true;
	case SaveTypeID::UInt64:
		if (null_for_empty && TYPED_DATA_IS_EMPTY(type, !*(const uint64_t*)data))
			return false;

		output = Json::Value(*(const uint64_t*)data);
		return true;
	case SaveTypeID::Float:
		if (null_for_empty && TYPED_DATA_IS_EMPTY(type, !*(const float*)data))
			return false;

		output = Json::Value(static_cast<double>(*(const float*)data));
		return true;
	case SaveTypeID::Double:
		if (null_for_empty && TYPED_DATA_IS_EMPTY(type, !*(const double*)data))
			return false;

		output = Json::Value(*(const double*)data);
		return true;
	case SaveTypeID::String: {
		const char* const* str = reinterpret_cast<const char* const*>(data);
		if (null_for_empty && TYPED_DATA_IS_EMPTY(type, *str == nullptr))
			return false;
		output = *str == nullptr ? Json::Value::nullSingleton() : string_is_high(*str) ? Json::Value(string_to_bytes(*str)) : Json::Value(Json::StaticString(*str));
		return true;
	}
	case SaveTypeID::FixedString:
		if (null_for_empty && TYPED_DATA_IS_EMPTY(type, !strlen((const char*)data)))
			return false;
		output = string_is_high((const char*)data) ? Json::Value(string_to_bytes((const char*)data)) : Json::Value(Json::StaticString((const char*)data));
		return true;
	case SaveTypeID::FixedArray: {
		const uint8_t* element = (const uint8_t*)data;
		size_t			  i;
		size_t			  element_size;
		save_type_t       element_type;

		if (type->type_resolver) {
			element_type = type->type_resolver();
			element_size = get_complex_type_size(element_type);
		}
		else {
			element_size = get_simple_type_size((SaveTypeID)type->tag);
			element_type = { (SaveTypeID)type->tag };
		}

		if (null_for_empty) {
			if (type->is_empty) {
				if (type->is_empty(data))
					return false;
			}
			else {
				for (i = 0; i < type->count; i++, element += element_size) {
					Json::Value value;
					bool valid_value = write_save_type_json(element, &element_type, !element_type.never_empty, value);

					if (valid_value)
						break;
				}

				if (i == type->count)
					return false;
			}
		}

		element = (const uint8_t*)data;
		Json::Value v(Json::arrayValue);

		for (i = 0; i < type->count; i++, element += element_size) {
			Json::Value value;
			write_save_type_json(element, &element_type, false, value);
			v.append(std::move(value));
		}

		output = std::move(v);
		return true;
	}
	case SaveTypeID::SavableDynamic: {
		const savable_allocated_memory_t<void, 0>* savptr = (const savable_allocated_memory_t<void, 0> *) data;
		size_t			  i;
		size_t			  element_size;
		save_type_t       element_type;

		if (type->type_resolver) {
			element_type = type->type_resolver();
			element_size = get_complex_type_size(element_type);
		}
		else {
			element_size = get_simple_type_size((SaveTypeID)type->tag);
			element_type = { (SaveTypeID)type->tag };
		}

		const uint8_t* element = (const uint8_t*)savptr->ptr;

		if (null_for_empty) {
			if (type->is_empty) {
				if (type->is_empty(data))
					return false;
			}
			else {
				for (i = 0; i < savptr->count; i++, element += element_size) {
					Json::Value value;
					bool valid_value = write_save_type_json(element, &element_type, !element_type.never_empty, value);

					if (valid_value)
						break;
				}

				if (i == savptr->count)
					return false;
			}
		}

		element = (const uint8_t*)savptr->ptr;
		Json::Value v(Json::arrayValue);

		for (i = 0; i < savptr->count; i++, element += element_size) {
			Json::Value value;
			write_save_type_json(element, &element_type, false, value);
			v.append(std::move(value));
		}

		output = std::move(v);
		return true;
	}
	case SaveTypeID::BitSet:
		return type->write(data, null_for_empty, output);
	case SaveTypeID::Struct: {
		if (type->is_empty && type->is_empty(data))
			return false;

		Json::Value obj;
		bool		valid_value = write_save_struct_json(data, type->structure, true, obj);

		if (null_for_empty && (!valid_value || !obj.size()))
			return false;

		output = std::move(obj);
		return true;
	}
	case SaveTypeID::Entity: {
		const gentity_t* entity = *reinterpret_cast<const gentity_t* const*>(data);

		if (null_for_empty && TYPED_DATA_IS_EMPTY(type, entity == nullptr))
			return false;

		if (!entity) {
			output = Json::Value::nullSingleton();
			return true;
		}

		output = Json::Value(entity->s.number);
		return true;
	}
	case SaveTypeID::ItemPointer: {
		const Item* item = *reinterpret_cast<const Item* const*>(data);

		if (item != nullptr && item->id != 0)
			if (!strlen(item->className))
				gi.Com_ErrorFmt("Attempt to persist invalid item {} (index {})", item->pickupName, (int32_t)item->id);

		if (null_for_empty && TYPED_DATA_IS_EMPTY(type, item == nullptr))
			return false;

		if (item == nullptr) {
			output = Json::Value::nullSingleton();
			return true;
		}

		output = Json::Value(Json::StaticString(item->className));
		return true;
	}
	case SaveTypeID::ItemIndex: {
		const item_id_t index = *reinterpret_cast<const item_id_t*>(data);

		if (index < IT_NULL || index >= IT_TOTAL)
			gi.Com_ErrorFmt("Attempt to persist invalid item index {}", (int32_t)index);

		const Item* item = GetItemByIndex(index);

		if (index)
			if (!strlen(item->className))
				gi.Com_ErrorFmt("Attempt to persist invalid item {} (index {})", item->pickupName, (int32_t)item->id);

		if (null_for_empty && TYPED_DATA_IS_EMPTY(type, item == nullptr))
			return false;

		if (item == nullptr) {
			output = Json::Value::nullSingleton();
			return true;
		}

		output = Json::Value(Json::StaticString(item->className));
		return true;
	}
	case SaveTypeID::Time: {
		const GameTime& time = *(const GameTime*)data;

		if (null_for_empty && TYPED_DATA_IS_EMPTY(type, !time))
			return false;

		output = Json::Value(time.milliseconds());
		return true;
	}
	case SaveTypeID::Data: {
		if (!data) {
			output = Json::Value::nullSingleton();
			return true;
		}

		const save_void_t& ptr = *reinterpret_cast<const save_void_t*>(data);

		if (null_for_empty && TYPED_DATA_IS_EMPTY(type, !ptr))
			return false;

		if (!ptr) {
			output = Json::Value::nullSingleton();
			return true;
		}

		if (!ptr.save_list()) {
			gi.Com_ErrorFmt("Attempt to persist invalid data pointer {} in list {}", ptr.pointer(), type->tag);
			return false;
		}

		output = Json::Value(Json::StaticString(ptr.save_list()->name));
		return true;
	}
	case SaveTypeID::Inventory: {
		Json::Value	   inventory = Json::Value(Json::objectValue);
		const int32_t* inventory_ptr = (const int32_t*)data;

		for (item_id_t i = static_cast<item_id_t>(IT_NULL + 1); i < IT_TOTAL; i = static_cast<item_id_t>(i + 1)) {
			Item* item = GetItemByIndex(i);

			if (!item || !item->className) {
				if (inventory_ptr[i])
					gi.Com_ErrorFmt("Item index {} is in inventory but has no className", (int32_t)i);

				continue;
			}

			if (inventory_ptr[i])
				inventory[item->className] = Json::Value(inventory_ptr[i]);
		}

		if (null_for_empty && inventory.empty())
			return false;

		output = std::move(inventory);
		return true;
	}
	case SaveTypeID::Reinforcements: {
		const reinforcement_list_t* reinforcement_ptr = (const reinforcement_list_t*)data;

		if (null_for_empty && !reinforcement_ptr->num_reinforcements)
			return false;

		Json::Value entries = Json::Value(Json::arrayValue);
		entries.resize(reinforcement_ptr->num_reinforcements);

		for (uint32_t i = 0; i < reinforcement_ptr->num_reinforcements; i++) {
			const reinforcement_t* reinforcement = &reinforcement_ptr->reinforcements[i];

			Json::Value obj = Json::Value(Json::objectValue);

			obj["classname"] = Json::StaticString(reinforcement->className);
			obj["mins"] = Json::Value(Json::arrayValue);
			obj["maxs"] = Json::Value(Json::arrayValue);
			for (int32_t x = 0; x < 3; x++) {
				obj["mins"][x] = reinforcement->mins[x];
				obj["maxs"][x] = reinforcement->maxs[x];
			}
			obj["strength"] = reinforcement->strength;
			obj["count"] = reinforcement_ptr->spawn_counts ? reinforcement_ptr->spawn_counts[i] : 0;

			entries[i] = obj;
		}

		Json::Value reinforcements = Json::Value(Json::objectValue);
		reinforcements["entries"] = std::move(entries);
		reinforcements["next"] = reinforcement_ptr->next_reinforcement;

		output = std::move(reinforcements);
		return true;
	}
	default:
		gi.Com_ErrorFmt("Can't persist type ID {}", (int32_t)type->id);
	}

	return false;
}

// write the specified data+structure to the JSON object
// referred to by `json`.
bool write_save_struct_json(const void* data, const save_struct_t* structure, bool null_for_empty, Json::Value& output) {
	Json::Value obj(Json::objectValue);

	for (auto& field : structure->fields) {
		if (!field.name) {
			gi.Com_PrintFmt("{}: save structure {} has unnamed field at offset {}\n", __FUNCTION__, structure->name, field.offset);
			continue;
		}

		const void* p = ((const uint8_t*)data) + field.offset;
		Json::Value value;
		bool		valid_value = write_save_type_json(p, &field.type, !field.type.never_empty, value);

		if (valid_value)
			obj[field.name].swap(value);
	}

	if (null_for_empty && obj.empty())
		return false;

	output = std::move(obj);
	return true;
}

// read the specified data+structure from the JSON object
// referred to by `json`.
void read_save_struct_json(const Json::Value& json, void* data, const save_struct_t* structure) {
	if (!json.isObject()) {
		json_print_error("", "expected object", false);
		return;
	}

	for (auto it = json.begin(); it != json.end(); it++) {
		const char* dummy;
		const char* key = it.memberName(&dummy);
		const Json::Value& value = *it;
		const auto field = std::find_if(structure->fields.begin(), structure->fields.end(), [key](const save_field_t& candidate) {
			return candidate.name && strcmp(key, candidate.name) == 0;
			});

		if (field == structure->fields.end()) {
			json_print_error(key, "unknown field", false);
			continue;
		}

		void* p = ((uint8_t*)data) + field->offset;
		read_save_type_json(value, p, &field->type, field->name);
	}
}

#include <fstream>
#include <memory>
#include <cstring>

static Json::Value parseJson(const char* jsonString) {
	Json::CharReaderBuilder reader;
	reader["allowSpecialFloats"] = true;
	Json::Value		  json;
	JSONCPP_STRING	  errs;
	std::stringstream ss(jsonString, std::ios_base::in | std::ios_base::binary);

	if (!Json::parseFromStream(reader, ss, &json, &errs))
		gi.Com_ErrorFmt("Couldn't decode JSON: {}", errs.c_str());

	if (!json.isObject())
		gi.Com_Error("expected object at root");

	return json;
}

class CountingStreamBuf : public std::streambuf {
public:
	CountingStreamBuf() : count(0) {}

	size_t Count() const {
		return count;
	}

protected:
	std::streamsize xsputn(const char* s, std::streamsize n) override {
		count += static_cast<size_t>(n);
		return n;
	}

	int overflow(int ch) override {
		if (ch != traits_type::eof()) {
			++count;
			return ch;
		}

		return traits_type::eof();
	}

private:
	size_t count;
};

class FixedBufferStreamBuf : public std::streambuf {
public:
	FixedBufferStreamBuf(char* buffer, size_t size) {
		setp(buffer, buffer + size);
	}

	size_t Written() const {
		return static_cast<size_t>(pptr() - pbase());
	}

protected:
	std::streamsize xsputn(const char* s, std::streamsize n) override {
		if (pptr() + n > epptr())
			n = epptr() - pptr();

		memcpy(pptr(), s, static_cast<size_t>(n));
		pbump(static_cast<int>(n));
		return n;
	}

	int overflow(int ch) override {
		if (ch != traits_type::eof() && pptr() < epptr()) {
			*pptr() = static_cast<char>(ch);
			pbump(1);
			return ch;
		}

		return traits_type::eof();
	}
};

/*
=============
saveJson
=============
*/
static char* saveJson(const Json::Value& json, size_t* out_size) {
	Json::StreamWriterBuilder builder;
	builder["indentation"] = "\t";
	builder["useSpecialFloats"] = true;
	const std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());

	CountingStreamBuf counting_buf;
	std::ostream counting_stream(&counting_buf);
	writer->write(json, &counting_stream);

	*out_size = counting_buf.Count();
	char* const out = static_cast<char*>(gi.TagMalloc(*out_size + 1, TAG_GAME));

	FixedBufferStreamBuf buffer(out, *out_size);
	std::ostream output_stream(&buffer);
	writer->write(json, &output_stream);

	*out_size = buffer.Written();
	out[*out_size] = '\0';
	return out;
}

// new entry point for WriteGame.
// returns pointer to TagMalloc'd JSON string.
char* WriteGameJson(bool autosave, size_t* out_size) {
	if (!autosave)
		SaveClientData();

	Json::Value json(Json::objectValue);

	WriteSaveMetadata(json);

	// write game
	game.autoSaved = autosave;
	write_save_struct_json(&game, &GameLocals_savestruct, false, json["game"]);
	game.autoSaved = false;

	// write clients
	Json::Value clients(Json::arrayValue);
	for (size_t i = 0; i < game.maxClients; i++) {
		Json::Value v;
		write_save_struct_json(&game.clients[i], &gclient_t_savestruct, false, v);
		clients.append(std::move(v));
	}
	json["clients"] = std::move(clients);

	return saveJson(json, out_size);
}

void PrecacheInventoryItems();

// new entry point for ReadGame.
// takes in pointer to JSON data. does
// not store or modify it.
void ReadGameJson(const char* jsonString) {
	const uint32_t maxEntities = game.maxEntities;
	const uint32_t max_clients = game.maxClients;

	FreeClientArray();
	gi.FreeTags(TAG_GAME);

	Json::Value json = parseJson(jsonString);

	if (!ValidateSaveMetadata(json, "game"))
		return;

	game = {};
	g_entities = (gentity_t*)gi.TagMalloc(maxEntities * sizeof(g_entities[0]), TAG_GAME);
	game.maxEntities = maxEntities;
	globals.gentities = g_entities;
	globals.maxEntities = game.maxEntities;

	AllocateClientArray(static_cast<int>(max_clients));

	// read game
	json_push_stack("game");
	read_save_struct_json(json["game"], &game, &GameLocals_savestruct);
	json_pop_stack();

	// read clients
	const Json::Value& clients = json["clients"];

	if (!clients.isArray())
		gi.Com_Error("expected \"clients\" to be array");
	else if (clients.size() != game.maxClients)
		gi.Com_Error("mismatched client size");

	size_t i = 0;

	for (auto& v : clients) {
		json_push_stack(fmt::format("clients[{}]", i));
		read_save_struct_json(v, &game.clients[i++], &gclient_t_savestruct);
		json_pop_stack();
	}

	PrecacheInventoryItems();
}

// new entry point for WriteLevel.
// returns pointer to TagMalloc'd JSON string.
char* WriteLevelJson(bool transition, size_t* out_size) {
	// update current level entry now, just so we can
	// use gamemap to test EOU
	UpdateLevelEntry();

	Json::Value json(Json::objectValue);

	WriteSaveMetadata(json);

	// write level
	write_save_struct_json(&level, &LevelLocals_savestruct, false, json["level"]);

	// write entities
	Json::Value entities(Json::objectValue);
	char		number[16];

	for (size_t i = 0; i < globals.numEntities; i++) {
		if (!globals.gentities[i].inUse)
			continue;
		// clear all the client inUse flags before saving so that
		// when the level is re-entered, the clients will spawn
		// at spawn points instead of occupying body shells
		else if (transition && i >= 1 && i <= game.maxClients)
			continue;

		auto result = std::to_chars(number, number + sizeof(number) - 1, i);

		if (result.ec == std::errc())
			*result.ptr = '\0';
		else
			gi.Com_ErrorFmt("error formatting number: {}", std::make_error_code(result.ec).message());

		write_save_struct_json(&globals.gentities[i], &gentity_t_savestruct, false, entities[number]);
	}

	json["entities"] = std::move(entities);

	return saveJson(json, out_size);
}

// new entry point for ReadLevel.
// takes in pointer to JSON data. does
// not store or modify it.
void ReadLevelJson(const char* jsonString) {
	// free any dynamic memory allocated by loading the level
	// base state
	gi.FreeTags(TAG_LEVEL);

	Json::Value json = parseJson(jsonString);

	if (!ValidateSaveMetadata(json, "level"))
		return;

	// wipe all the entities
        memset(static_cast<void *>(g_entities), 0,
               game.maxEntities * sizeof(g_entities[0]));
	globals.numEntities = game.maxClients + 1;

	// read level
	json_push_stack("level");
	read_save_struct_json(json["level"], &level, &LevelLocals_savestruct);
	json_pop_stack();

	// read entities
	const Json::Value& entities = json["entities"];

	if (!entities.isObject())
		gi.Com_Error("expected \"entities\" to be object");

	//for (auto key : json.getMemberNames())
	for (auto it = entities.begin(); it != entities.end(); it++) {
		const char* dummy;
		const char* id = it.memberName(&dummy);
		const Json::Value& value = *it;//json[key];
		uint32_t		   number = strtoul(id, nullptr, 10);

		if (number >= globals.numEntities)
			globals.numEntities = number + 1;

		gentity_t* ent = &g_entities[number];
		InitGEntity(ent);
		json_push_stack(fmt::format("entities[{}]", number));
		read_save_struct_json(value, ent, &gentity_t_savestruct);
		json_pop_stack();
		gi.linkEntity(ent);
	}

	// mark all clients as unconnected
	for (size_t i = 0; i < game.maxClients; i++) {
		gentity_t* ent = &g_entities[i + 1];
		ent->client = game.clients + i;
		ent->client->pers.connected = false;
		ent->client->pers.limitedLivesPersist = false;
		ent->client->pers.limitedLivesStash = 0;
		ent->client->pers.spawned = false;
	}

	// do any load time things at this point
	for (size_t i = 0; i < globals.numEntities; i++) {
		gentity_t* ent = &g_entities[i];

		if (!ent->inUse)
			continue;

		// fire any cross-level/unit triggers
		if (ent->className)
			if (strcmp(ent->className, "target_crosslevel_target") == 0 ||
				strcmp(ent->className, "target_crossunit_target") == 0)
				ent->nextThink = level.time + GameTime::from_sec(ent->delay);
	}

	PrecacheInventoryItems();

	// clear cached indices
	cached_soundIndex::reset_all();
	cached_modelIndex::reset_all();
	cached_imageIndex::reset_all();

	G_LoadShadowLights();
}

// [Paril-KEX]
bool CanSave() {
	if (game.maxClients == 1 && g_entities[1].health <= 0) {
		gi.LocClient_Print(&g_entities[1], PRINT_CENTER, "$g_no_save_dead");
		return false;
	}
	// don't allow saving during cameras/intermissions as this
	// causes the game to act weird when these are loaded
	else if (level.intermission.time) {
		return false;
	}

	return true;
}

/*static*/ template<> cached_soundIndex* cached_soundIndex::head = nullptr;
/*static*/ template<> cached_modelIndex* cached_modelIndex::head = nullptr;
/*static*/ template<> cached_imageIndex* cached_imageIndex::head = nullptr;

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <cstdint>

typedef enum {
    P_bad,

    P_prethink,
    P_think,
    P_blocked,
    P_touch,
    P_use,
    P_pain,
    P_die,

    P_moveinfo_endfunc,

    P_monsterinfo_currentmove,
    P_monsterinfo_stand,
    P_monsterinfo_idle,
    P_monsterinfo_search,
    P_monsterinfo_walk,
    P_monsterinfo_run,
    P_monsterinfo_dodge,
    P_monsterinfo_attack,
    P_monsterinfo_melee,
    P_monsterinfo_sight,
    P_monsterinfo_checkattack
} ptr_type_t;

typedef struct {
    ptr_type_t type;
    std::uintptr_t value;
} save_ptr_t;

template <typename T>
constexpr std::uintptr_t save_ptr_encode(T ptr) noexcept
{
    return reinterpret_cast<std::uintptr_t>(ptr);
}

template <typename T>
constexpr T save_ptr_decode(std::uintptr_t value) noexcept
{
    return reinterpret_cast<T>(value);
}

extern const save_ptr_t save_ptrs[];
extern const int num_save_ptrs;

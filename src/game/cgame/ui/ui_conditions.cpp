#include "ui/ui_internal.h"
#include "ui/ui_cgame_access.h"

namespace ui {
namespace {

static bool EvaluateCvarCondition(const MenuCondition &condition)
{
    cvar_t *var = Cvar_FindVar(condition.name.c_str());
    if (!var)
        return false;

    if (condition.op == ConditionOp::Exists) {
        if (COM_IsFloat(var->string))
            return var->value != 0.0f;
        return var->string[0] != '\0';
    }

    const char *var_value = var->string ? var->string : "";
    const char *cond_value = condition.value.c_str();
    bool var_is_num = COM_IsFloat(var_value);
    bool cond_is_num = COM_IsFloat(cond_value);

    if (var_is_num && cond_is_num) {
        double lhs = atof(var_value);
        double rhs = atof(cond_value);
        switch (condition.op) {
        case ConditionOp::Equal:
            return lhs == rhs;
        case ConditionOp::NotEqual:
            return lhs != rhs;
        case ConditionOp::Greater:
            return lhs > rhs;
        case ConditionOp::GreaterEqual:
            return lhs >= rhs;
        case ConditionOp::Less:
            return lhs < rhs;
        case ConditionOp::LessEqual:
            return lhs <= rhs;
        default:
            return false;
        }
    }

    if (condition.op == ConditionOp::Equal || condition.op == ConditionOp::NotEqual) {
        bool match = Q_stricmp(var_value, cond_value) == 0;
        return condition.op == ConditionOp::Equal ? match : !match;
    }

    return false;
}

static bool EvaluateCondition(const MenuCondition &condition)
{
    bool result = false;
    switch (condition.kind) {
    case ConditionKind::InGame:
        result = CgameIsInGame();
        break;
    case ConditionKind::Deathmatch: {
        const char *style = CgameConfigString(CS_GAME_STYLE);
        int style_value = style ? atoi(style) : 0;
        result = style_value != 0;
        break;
    }
    case ConditionKind::Cvar:
        result = EvaluateCvarCondition(condition);
        break;
    }

    if (condition.negate)
        result = !result;
    return result;
}

} // namespace

void Widget::SetShowConditions(std::vector<MenuCondition> conditions)
{
    showConditions_ = std::move(conditions);
}

void Widget::SetEnableConditions(std::vector<MenuCondition> conditions)
{
    enableConditions_ = std::move(conditions);
}

void Widget::SetDefaultConditions(std::vector<MenuCondition> conditions)
{
    defaultConditions_ = std::move(conditions);
}

void Widget::UpdateConditions()
{
    hiddenByCondition_ = !UI_EvaluateConditions(showConditions_);
    disabledByCondition_ = !UI_EvaluateConditions(enableConditions_);
    defaultByCondition_ = !defaultConditions_.empty() && UI_EvaluateConditions(defaultConditions_);
}

bool UI_EvaluateConditions(const std::vector<MenuCondition> &conditions)
{
    if (conditions.empty())
        return true;

    for (const auto &condition : conditions) {
        if (!EvaluateCondition(condition))
            return false;
    }
    return true;
}

} // namespace ui

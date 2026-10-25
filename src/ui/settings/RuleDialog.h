#pragma once

#include <windows.h>

#include <string>
#include <utility>
#include <vector>

#include "core/Model.h"

namespace of {

// Modal editor for one sorting rule. `tabs` lists (tab id, label) of fences that can receive items.
// Returns true if the user pressed OK (rule updated in place).
bool EditRule(HWND owner, Rule& rule, const std::vector<std::pair<std::string, std::wstring>>& tabs);

}  // namespace of

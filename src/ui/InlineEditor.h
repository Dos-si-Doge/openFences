#pragma once

#include <windows.h>

#include <functional>
#include <string>

#include "core/Geometry.h"

namespace of {

// A borderless popup edit box used for renaming items and fence titles.
namespace inline_editor {

// onCommit runs when the user presses Enter or clicks away (not on Esc).
void Show(HWND owner, const RectI& screenRect, const std::wstring& text, float fontPx, bool selectStem,
          std::function<void(const std::wstring&)> onCommit);
void Cancel();
bool Active();

}  // namespace inline_editor

}  // namespace of

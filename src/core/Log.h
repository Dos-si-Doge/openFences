#pragma once

#include <format>
#include <string>

namespace of::log {

void Init(bool verbose);
void Write(const std::wstring& line);
bool Verbose();

template <typename... Args>
void Info(std::wformat_string<Args...> fmt, Args&&... args) {
    Write(std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void Debug(std::wformat_string<Args...> fmt, Args&&... args) {
    if (Verbose()) Write(std::format(fmt, std::forward<Args>(args)...));
}

}  // namespace of::log

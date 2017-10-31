#pragma once

#include <functional>

int WrapSyscall(const char* desc, std::function<int()> exec);

#pragma once

#include <string>

std::string ClangToolPath(const std::string& name);
std::string ClangCompileCommand(const std::string& filename,
                                const std::string& src_file,
                                const std::string& dst_file);

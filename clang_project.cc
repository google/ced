#include "absl/strings/str_cat.h"
#include "compilation_database.h"
#include "project.h"

class ClangProject final : public ProjectAspect, public CompilationDatabase {
 public:
  ClangProject(const std::string& cc) : compile_commands_(cc) {}

  std::string CompileCommandsFile() const override { return compile_commands_; }

 private:
  std::string compile_commands_;
};

IMPL_PROJECT_ASPECT(Clang, path, -100) {
  auto cc = absl::StrCat(path, "/compile_commands.json");
  FILE* f = fopen(cc.c_str(), "r");
  if (!f) return nullptr;
  fclose(f);

  return std::unique_ptr<ProjectAspect>(new ClangProject(cc));
}

#include "absl/strings/str_cat.h"
#include "project.h"

class CedProject final : public ProjectAspect,
                         public ProjectRoot,
                         public ConfigFile {
 public:
  CedProject(const std::string& root) : root_(root) {}

  std::string Path() const override { return root_; }
  std::string Config() const override { return absl::StrCat(root_, "/.ced"); }

 private:
  std::string root_;
};

IMPL_PROJECT_ASPECT(Ced, path, 1000) {
  FILE* f = fopen(absl::StrCat(path, "/.ced").c_str(), "r");
  if (!f) return nullptr;
  fclose(f);

  return std::unique_ptr<ProjectAspect>(new CedProject(path));
}

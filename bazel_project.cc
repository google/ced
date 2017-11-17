#include "absl/strings/str_cat.h"
#include "project.h"

class BazelProject final : public ProjectAspect, public ProjectRoot {
 public:
  BazelProject(const std::string& root) : root_(root) {}

  std::string Path() const override { return root_; }

 private:
  std::string root_;
};

IMPL_PROJECT_ASPECT(Bazel, path) {
  FILE* f = fopen(absl::StrCat(path, "/WORKSPACE").c_str(), "r");
  if (!f) return nullptr;
  fclose(f);

  return std::unique_ptr<ProjectAspect>(new BazelProject(path));
}

#include <string>

// can be used as a project aspect
class CompilationDatabase {
 public:
  virtual std::string CompileCommandsFile() const = 0;
};

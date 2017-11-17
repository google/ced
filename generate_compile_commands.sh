#!/bin/sh
set -ex
bazel build -c opt compilation_database
cat bazel-bin/compile_commands.json | sed "s,__EXEC_ROOT__,`bazel info execution_root`,g" > compile_commands.json


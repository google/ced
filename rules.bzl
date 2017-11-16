# Copyright 2017 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

def file2lib(name, src):
  native.genrule(
    name = "%s_cc" % name,
    srcs = [src],
    outs = ['%s.cc'% name],
    tools = ['//:file2c'],
    cmd = './$(location :file2c) $(OUTS) %s $(locations %s)' % (name, src)
  )

  native.genrule(
    name = "%s_h" % name,
    outs = ['%s.h' % name],
    cmd = 'echo -e "#pragma once\nextern const char* %s;\n" > $(OUTS)' % name
  )

  native.cc_library(
    name = name,
    srcs = ['%s.cc' % name],
    hdrs = ['%s.h' % name]
  )

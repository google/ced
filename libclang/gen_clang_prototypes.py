#!/usr/bin/python2.7
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

import sys
import re

_RE_API = r'(?:CINDEX_LINKAGE)([^;]*);'

def list_c_apis(filenames):
  for filename in filenames:
    with open(filename, 'r') as f:
      text = f.read()
    for m in re.finditer(_RE_API, text):
      api_declaration = re.sub('[ \r\n\t]+', ' ', m.group(1))
      type_and_name, args_and_close = api_declaration.split('(', 1)
      args = args_and_close[:args_and_close.rfind(')')].strip()
      last_space = type_and_name.rfind(' ')
      last_star = type_and_name.rfind('*')
      type_end = max(last_space, last_star)
      return_type = type_and_name[0:type_end+1].strip()
      name = type_and_name[type_end+1:].strip()
      if 'CXCursorAndRangeVisitorBlock' in args: continue
      if 'CXCursorVisitorBlock' in args: continue
      yield {'return_type': return_type, 'name': name, 'arguments': args, 'header': filename}

C = open(sys.argv[1], 'w')
H = open(sys.argv[2], 'w')

print >>C, '#include "libclang/libclang.h"'
print >>C, '#include <dlfcn.h>'
for f in sys.argv[3:]:
  print >>H, '#include "clang-c/%s"' % f[f.rfind('/')+1:]
print >>H, 'struct LibClang {'
print >>H, '  LibClang(const char* so);'
print >>H, '  ~LibClang();'
print >>H, '  LibClang(const LibClang&) = delete;'
print >>H, '  LibClang& operator=(const LibClang&) = delete;'
print >>H, '  void* dlhdl = nullptr;'
for api in list_c_apis(sys.argv[3:]):
  print >>H, 'typedef %(return_type)s (*%(name)s_type)(%(arguments)s);' % api
  print >>H, '%(name)s_type %(name)s = nullptr;' % api
print >>H, '};'

print >>C, 'LibClang::LibClang(const char* so) {'
print >>C, '  dlhdl = dlopen(so, RTLD_LAZY | RTLD_LOCAL | RTLD_NODELETE);'
print >>C, '  if (dlhdl == nullptr) return;'
for api in list_c_apis(sys.argv[3:]):
  print >>C, '  this->%(name)s = reinterpret_cast<%(name)s_type>(dlsym(dlhdl, "%(name)s"));' % api
print >>C, '}'
print >>C, 'LibClang::~LibClang() {'
print >>C, '  if (dlhdl) dlclose(dlhdl);'
print >>C, '}'


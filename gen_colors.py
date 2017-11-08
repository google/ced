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
import yaml
import sys

colors = yaml.load(open(sys.argv[1]))

with open(sys.argv[2], 'w') as f:
  print >>f, '#pragma once'
  print >>f, 'enum class ColorID : short {unused, %s};' % ','.join(colors.keys())
  print >>f, 'void InitColors();'

with open(sys.argv[3], 'w') as f:
  print >>f, '#include "colors.h"'
  print >>f, '#include <curses.h>'
  print >>f, 'void InitColors() {'
  print >>f, 'const short COLOR_DEFAULT = -1;'
  for color, fgbg in colors.items():
    print >>f, 'init_pair(static_cast<short>(ColorID::%s), COLOR_%s, COLOR_%s);' % (color, fgbg[0], fgbg[1])
  print >>f, '}'

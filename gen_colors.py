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

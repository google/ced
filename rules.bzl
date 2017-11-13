
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

#! /usr/bin/python
#
# This simple script scans a simple c soure file generates a set of stubs.
#
# (c) 2018 by Sebastian Bauer
#
from __future__ import print_function

from pycparser import parse_file, c_ast, c_parser, c_generator
from pycparser.c_ast import FuncDecl, TypeDecl

import re
import subprocess
import tempfile

class FuncDefVisitor(c_ast.NodeVisitor):
  def visit_Decl(self, node):
    if isinstance(node.type, FuncDecl):
      generator = c_generator.CGenerator()
      return_type = node.type.type
      return_type_str = generator.visit(return_type)

      print(generator.visit(node))
      print('{')
      print("  fprintf(stderr, \"%s called\", __PRETTY_FUNCTION__);")
      print("  exit(1);")
      if (not isinstance(return_type, TypeDecl)) or (not return_type_str == 'void'):
        print('  return 0;')
      print('}\n')

if __name__ == '__main__':
  includes = subprocess.check_output("echo '#define __attribute__(x)\n#define __extension__(x)\n#define __asm__(x)\n' | cat - indep-include/* | gcc -DNO_SSL -Dsize_t=int -E -xc - -I indep-include/ | grep -v ^\#", shell=True)
  includes = re.sub(r'__attribute__\s*\(\(.*[^\)]\)\)','',includes)
  includes = re.sub(r'__nptr','',includes)
  includes = re.sub(r'__endptr','',includes)
  includes = re.sub(r'__extension__','',includes)
  includes = re.sub(r'__inline','',includes)
  includes = re.sub(r'__restrict','',includes)
  includes = re.sub(r'__builtin_va_list','int',includes)

  ast = c_parser.CParser().parse(includes)
  v = FuncDefVisitor()
  v.visit(ast)

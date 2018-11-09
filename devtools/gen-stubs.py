#! /usr/bin/python
#
# This simple script scans a simple c soure file generates a set of stubs.
#
# (c) 2018 by Sebastian Bauer
#
from __future__ import print_function

from pycparser import parse_file, c_ast, c_parser, c_generator
from pycparser.c_ast import FuncDecl, TypeDecl

import argparse
import re
import subprocess
import tempfile

class FuncDefVisitor(c_ast.NodeVisitor):
  def __init__(self, functions):
    # type: (List[str]) -> None
    self.functions = functions

  def visit_Decl(self, node):
    if isinstance(node.type, FuncDecl):
      generator = c_generator.CGenerator()
      if node.name not in self.functions:
        return

      return_type = node.type.type
      return_type_str = generator.visit(return_type)

      print(generator.visit(node))
      print('{')
      print("  fprintf(stderr, \"%s called\\n\", __PRETTY_FUNCTION__);")
      print("  exit(1);")
      print('}\n')

if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('functions', help="The file the contains, line by line, the function for which the stubs shall be generated")
  args = parser.parse_args()

  with open(args.functions) as f:
    functions = set([l.strip() for l in f.readlines()])
#
  includes = subprocess.check_output("echo '#define __attribute__(x)\n#define __extension__(x)\n#define __asm__(x)\n' | cat - indep-include/* | gcc -DNO_SSL -Dsize_t=int -E -xc - -I indep-include/ | grep -v ^\#", shell=True)
  includes = re.sub(r'__attribute__\s*\(\(.*[^\)]\)\)','',includes)
  includes = re.sub(r'__nptr','',includes)
  includes = re.sub(r'__endptr','',includes)
  includes = re.sub(r'__extension__','',includes)
  includes = re.sub(r'__inline','',includes)
  includes = re.sub(r'__restrict','',includes)
  includes = re.sub(r'__builtin_va_list','int',includes)

  ast = c_parser.CParser().parse(includes)

  print('#include <stdio.h>')
  print('#include <stdlib.h>')
  print()
  print('#include "codesets.h"')
  v = FuncDefVisitor(functions)
  v.visit(ast)

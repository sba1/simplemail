#! /usr/bin/python
#
# Generates test data embedded into the source file that is read from stdin
#
from __future__ import print_function

import re
import os.path
import sys
import argparse

parser = argparse.ArgumentParser(description="""
  Generates test data embedded into the source file that is read from stdin
  or from the file specified with --inputfile.
  """)

parser.add_argument('--inputfile', help="""
  Specifies the name of the file that is used for scanning instead of stdin.
  """)

parser.add_argument('--outdir',  help="""
  Write the files into the specified output direcory. If this doesn't exists
  it is creatde.
  """)

parser.add_argument('--makefile', help="""
  Specifies the file name of a makefile that will be additionally generated
  and will contain the dependencies. Only works if --inputfile is specified.
  """)

args = parser.parse_args()

inputfile = sys.stdin
if args.inputfile:
  inputfile = open(args.inputfile, 'r')

outdir = ''
if args.outdir is not None:
  outdir = args.outdir
  if not os.path.exists(args.outdir):
    os.mkdir(outdir)

makefile = None
if args.makefile is not None:
  if args.inputfile is None:
    sys.exit('--makefile needs --input')
  makefile = open(args.makefile,'w')

text = inputfile.read()

result = re.findall(r'@Test[^@.]*@File\s\"([^\"]+)\"\w*\n(.*?){{{\n(.*?)}}}', text, re.DOTALL)

for r in result:
    fn = os.path.join(outdir, r[0])
    prefix = r[1]
    contents = r[2]
    # Extract the proper lines, i.e., without the prefix and without the last line
    lines = [l[len(prefix):] + "\n" for l in contents.split('\n')[:-1]]
    sys.stderr.write('Generating {0}\n'.format(fn))
    with open(fn,'w') as f:
        f.writelines(lines)

    if makefile:
      print('{0}: {1}'.format(fn, args.inputfile), file=makefile)
      print(file=makefile)
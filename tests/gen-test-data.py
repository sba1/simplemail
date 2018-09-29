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
  Generates test data embedded into the source file that is read from stdin.
  """)

parser.add_argument('--outdir',  help="""
  Write the files into the specified output direcory. If this doesn't exists
  it is creatde.
  """)

args = parser.parse_args()

outdir = ''
if args.outdir is not None:
  outdir = args.outdir
  if not os.path.exists(args.outdir):
    os.mkdir(outdir)

text = sys.stdin.read()

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

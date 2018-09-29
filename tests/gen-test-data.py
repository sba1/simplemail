#! /usr/bin/python
#
# Generates test data embedded into the source file that is read from stdin
#
import re
import sys

text = sys.stdin.read()

result = re.findall(r'@Test.*@File\s\"(.+)\".*\n(.*){{{\n(.*)}}}', text, re.DOTALL)

for r in result:
    fn = r[0]
    prefix = r[1]
    contents = r[2]
    # Extract the proper lines, i.e., without the prefix and without the last line
    lines = [l[len(prefix):] + "\n" for l in contents.split('\n')[:-1]]
    sys.stderr.write('Generating {0}\n'.format(fn))
    with open(r[0],'w') as f:
        f.writelines(lines)

#!/use/bin/python3

import argparse
import json
import re
import sys

def parse_ninja_file(f):
  result = {}
  targets = []
  current_target = None
  target_output_name = None
  in_build_block = False
  for line in f:
    if not in_build_block:
      line = line.strip()
      if not line:
        continue
      if line.startswith('defines = '):
        result['defines'] = line.split('=', 1)[1].strip()
      elif line.startswith('label_name = '):
        result['label_name'] = line.split('=', 1)[1].strip()
      elif line.startswith('root_out_dir = '):
        result['root_out_dir'] = line.split('=', 1)[1].strip()
      elif line.startswith('target_out_dir = '):
        result['target_out_dir'] = line.split('=', 1)[1].strip()
      elif line.startswith('target_output_name = '):
        target_output_name = line.split('=', 1)[1].strip()
        result['target_output_name'] = target_output_name
      elif line.startswith('build '):
        output = line.split(':', 1)[0].split()[1]
        solib = '%(root_out_dir)s/%(target_output_name)s.so' % result
        if output == solib:
          in_build_block = True
          result['solibs'] = [solib]
    else:
      line = line.strip()
      if not line:
        in_build_block = False
      elif line.startswith('solibs'):
        result['solibs'] += line.split(' = ', 1)[1].split()

  return result

def main():
    parser = argparse.ArgumentParser(description='A simple argument parser')
    parser.add_argument('-i', '--input', type=str, help='the input .ninja files')
    parser.add_argument('-o', '--output', type=str, help='the output json file')

    args = parser.parse_args()
    if not args.input:
      parser.print_help()

    if not args.output:
      parser.print_help()

    ninja_files = args.input.split(',')

    try:
      ninja_files = [open(f, 'r') for f in ninja_files]
    except:
      print('Cannot open ninja files: %s' % args.input)
      return -1

    solibs = []
    for f in ninja_files:
      result = parse_ninja_file(f)
      solibs += result['solibs']

    solibs = list(set(solibs))
    solibs.sort()

    with open(args.output, 'w') as f:
      f.write(json.dumps({'solibs': solibs}))

    return 0

if __name__ == "__main__":
  sys.exit(main())
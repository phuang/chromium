#!/usr/bin/python

import argparse
import json
import re
import os.path
import shutil
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
    parser.add_argument('-b', '--build-root', type=str, help='the output directory')
    parser.add_argument('-n', '--ninjas', type=str, help='the .ninja files')
    parser.add_argument('-d', '--copy-dest', type=str, help='the output directory')

    args = parser.parse_args()
    if not args.build_root:
      parser.print_help()
      return -1

    if not args.ninjas:
      parser.print_help()
      return -1

    if not args.copy_dest:
      parser.print_help()
      return -1

    try:
      ninja_files = []
      for ninja in args.ninjas.split(','):
        path = os.path.join(args.build_root, ninja)
        ninja_files.append(open(path, 'r'))
    except:
      print('Cannot open ninja files: %s' % args.ninja)
      return -1

    solibs = []
    for ninja_file in ninja_files:
        solibs += parse_ninja_file(ninja_file)['solibs']

    for solib in solibs:
      src_path = os.path.join(args.build_root, solib)
      dst_path = os.path.join(args.copy_dest, solib)
      print("copy %s to %s" % (src_path, dst_path))
      shutil.copy(src_path, dst_path)

    return 0

if __name__ == "__main__":
  sys.exit(main())

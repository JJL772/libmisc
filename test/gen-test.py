#!/usr/bin/env python3

import os
import sys

with open('large_test.kv', 'w') as fp:
	fp.write('test\n{')
	for i in range(1,10000000):
		fp.write(f'\t"key_{i}" "value {i}"\n')
	fp.write('}\n')
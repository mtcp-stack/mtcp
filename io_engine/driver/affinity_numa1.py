#!/usr/bin/env python

import os
import sys
import subprocess

def execute(cmd):
	try:
		proc = subprocess.Popen(cmd, shell = True, stdout = subprocess.PIPE)
		return proc.communicate()[0]
	except:
		pass
		return None

if os.getuid() != 0:
	print 'You must be root!'
	sys.exit(1)

num_cpus = len(execute('cat /proc/cpuinfo | grep processor').strip().split('\n'))

if len(sys.argv) < 2:
	print 'usage: %s <interface name>' % sys.argv[0]
	sys.exit(1)

ifname = sys.argv[1]

intrmap = execute('cat /proc/interrupts | grep %s-rx-' % ifname).strip().split('\n')
	
for intr in intrmap:
	irq = int(intr.split()[0][:-1])
	name = intr.split()[-1]
	queue = int(name[name.rfind('-') + 1:])

	cpu = queue + 6

	print 'echo %x > /proc/irq/%d/smp_affinity' % (1 << cpu, irq)
	execute('echo %x > /proc/irq/%d/smp_affinity' % (1 << cpu, irq))

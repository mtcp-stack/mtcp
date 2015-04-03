#!/usr/bin/env python

import sys
import os
import subprocess
import time

#ITR = 12500 #956	# interrupt throttling rate
ITR = 956

def execute(cmd):
	try:
		proc = subprocess.Popen(cmd, shell = True, stdout = subprocess.PIPE)
		return proc.communicate()[0]
	except:
		pass
		return None
	
def get_num_interfaces():
	output_82598 = execute('lspci | grep 82598').strip()
	num_82598 = len(output_82598.split('\n'))
	if output_82598 == '':
		num_82598 = 0

	output_82599 = execute('lspci | grep 82599').strip()
	num_82599 = len(output_82599.split('\n'))
	if output_82599 == '':
		num_82599 = 0
	
	return num_82598 + num_82599

def get_num_cpus():
	output = execute('cat /proc/cpuinfo | grep processor')
	return len(output.strip().split('\n'))
	
if os.getuid() != 0:
	print 'You must be root!'
	sys.exit(1)

if len(sys.argv) < 3:
	print 'usage: %s <# of RX queues> <# of TX queues>' % sys.argv[0]
	print '  You can specify 0 instead of the number of queues (one queue for each core)'
	sys.exit(1)

num_rx_queues = int(sys.argv[1])
num_tx_queues = int(sys.argv[2])
postfix = '3'

assert 0 <= num_rx_queues <= 16

num_ifs = get_num_interfaces()
num_cpus = get_num_cpus()

execute('lsmod | grep ps_ixgbe > /dev/null && sudo rmmod ps_ixgbe')
execute('insmod ./ps_ixgbe.ko RXQ=%s TXQ=%s InterruptThrottleRate=%s' % 
		(','.join([str(num_rx_queues)] * num_ifs),
		 ','.join([str(num_tx_queues)] * num_ifs),
		 ','.join([str(ITR)] * num_ifs))
	)

time.sleep(3)
#for i in range(num_ifs):
for i in range(2):
	ifname = 'xge%d' % i
	print 'setting %s...' % ifname
	
	execute('ethtool -A %s autoneg off rx off tx off' % ifname)
	execute('ifconfig %s 10.0.%d.%s mtu 1500 netmask 255.255.255.0' % (ifname, i, postfix))

	print 'OK'
	print execute('./affinity.py %s' % ifname).strip()

execute('rm -f /dev/packet_shader')
execute('mknod /dev/packet_shader c 1010 0')
execute('chmod 666 /dev/packet_shader')

Throughput Measurement Application
==================================

As the name suggests, this application is an iperf-like application which
attempts to measure throughput when using mTCP by measuring the time it takes to
complete a bulk transfer. 

At a high level, the application works as follows:
1. The sender (running ./client.c) sends X bytes to the receiver (running recv.py)
2. The sender sends DONE
3. Upon receiving DONE, the sender responds OK, and reports throughput
4. Upon receiving OK, the sender reports throughput and exits

The sender reports throughput as bytes / (time elapsed between first byte sent and OK recevied)
The receiver reports throughput as bytes / (time elapsed between first byte receiver and OK sent)


Example Experiment
==================

[Setup]

0. Ensure the ARP table and routing table are populated correctly and located
	 inside ./config/
	 
1. Set the congestion window: edit mtcp/src/tcp_cong.c and change line 7 to
	 return the fixed cwnd (e.g. 20000) in bytes.

2. Re-make mTCP, then build this application by simply running make in this
	 directory (apps/tpt)

3. Assuming sender and receiver are on the same network and/or have a very low
   delay between them, add a netem qdisc for ingress traffic at the receiver
   (who is not running mTCP/DPDK) to simulate a reasonable delay for the link.
   To add 20 ms delay to interface ETH, use the included script as follows:

       ./add-delay.sh ETH 20

	 (you can also easily remove the qdiscs later with ./rm-delay.sh ETH)

[Run]

2. Start the receiver on a known port (say 9000)

       python recv.py 9000

3. Start the client, which will send SEND_BYTES bytes to the receiver

       sudo ./client RECEIVER_IP 9000 SEND_BYTES



Current Observations
====================

(Sender running mTCP over DPDK on a MLX4 NIC) 
(Receiver using kernel's TCP stack)
(Both machines running Ubuntu 16.04, Kernel 4.4)

1. CWND=20,000 DELAY=20ms SEND_BYTES=1000000 (1MB)

@sender:
  time_elapsed = 0.000245 sec 
  throughput = 32901.747 Mbit/sec

@receiver:
  time_elapsed = 1.068184 sec
  throughput = 7.546 Mbit/sec

With a fixed CWND of 20,000 bytes and a fixed RTT of 20ms, we would expect
throughput to be roughly:

CWMD / RTT = 20,000 bytes / 20 ms = 1 Mbit/s

The log reports that the srtt is being estimated correctly:
[RTT: EstimateRTT: 301] mrtt: 20 (20000us), srtt: 167 (20ms), mdev: 3, mdev_max: 3, rttvar: 6, rtt_seq: 10928389

and that the congestion window is staying roughly around the fixed value I chose:
[CONG: ProcessACK: 506] congestion avoidance cwnd: 41438, ssthresh: 14600



2. CWND=20,000 (bytes) DELAY=20 (ms) SEND_BYTES=10000000 (10MB)

@sender:
  time_elapsed = 24.370979 sec
  throughput = 3.283 Mbit/sec

@receiver:
  time_elapsed = 28.823848 sec
  throughput = 2.776 MBit/sec

For some reason, increasing the amount of data sent decreased the throughput.



3. CWND=40,000 (bytes) DELAY=20 (ms) SEND_BYTES=10000000 (10MB)

@sender:
  time_elapsed = 11.601138 sec
  throughput = 6.899 Mbit/sec

@receiver:
  time_elapsed = 13.719352 sec
  throughput = 5.834 MBit/sec

This suggests that the CWND has the expected linear relationship to
throughput (i.e. doubling CWND roughly doubled throughput), but it still
differs from what theory tells us throughput should be: 40,000 / 20 = 2 Mbit/sec

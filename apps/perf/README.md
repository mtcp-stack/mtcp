Throughput Measurement Application
==================================

As the name suggests, this application is an iperf-like application which
attempts to measure throughput with mTCP by measuring the time it takes to
complete a bulk transfer. 

The client uses mTCP sockets, while the receiver is written in Python using
traditional sockets so that it can be run on a machine that does not have
DPDK/mTCP installed. 

The traffic always flows from client to receiver, but there are two different
modes that allow you to specify whether the client or receiver should initiate
the connection:

1. Client initiates connection (receiver listens on `port` and client sends to
`ip:port` for `length` seconds)
- `python recv.py wait [ip] [port]`
- `./client send [ip] [port] [length (seconds)]` 

2. Receiver initiates connection (client listens on `port`, receiver sends a
start message to `ip:port`, and then client sends back to receiver for `length`
seconds)
- `./client wait [ip] [port] [length (seconds)]`
- `python recv.py send [ip] [port]`

*NOTE*: If using CCP with mTCP, you will need to ensure that `LD_LIBRARY_PATH`
includes the path to libccp:

`export LD_LIBRARY_PATH=/path/to/mtcp/src/libccp:$LD_LIBRARY_PATH`

Even once this is in your user's local environment, you will probably need to
make sure it persists when running with sudo:

`sudo env LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./client ...`


Setup Notes
===========

0. Ensure the ARP table and routing table are populated correctly and located inside ./config/

1. Build this application by simply running make in this directory (apps/perf)

2. Assuming sender and receiver are on the same network and/or have a very low
   delay between them, add a netem qdisc for ingress traffic at the receiver
   (who is not running mTCP/DPDK) to simulate a reasonable delay for the link.
   To add 20 ms delay to interface ETH, use the included script as follows:

        ./add-delay.sh IFACE 20

    (you can also easily remove the qdiscs later with ./rm-delay.sh ETH)

3. Start mTCP perf client in wait mode, listening on, e.g., port 9000 and
sending for 30 seconds:

`sudo env LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./client wait 10.1.1.5 9000 30`

4. Start python receiver in send mode, where client ip is e.g. 10.1.1.5

`sudo python recv.py send 10.1.1.5 9000`

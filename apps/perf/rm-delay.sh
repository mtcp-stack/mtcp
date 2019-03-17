ETH=$1

sudo tc qdisc del dev $ETH ingress
sudo tc qdisc del dev ifb0 parent 1: handle 2: netem 
sudo tc qdisc del dev ifb0 root

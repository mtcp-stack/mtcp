ETH=$1
DELAY=$2

sudo modprobe ifb
ip link set dev ifb0 up
sudo tc qdisc add dev ${ETH} ingress
sudo tc filter add dev ${ETH} parent ffff: protocol ip u32 match u32 0 0 flowid 1:1 action mirred egress redirect dev ifb0
sudo tc qdisc add dev ifb0 root netem delay ${DELAY}ms


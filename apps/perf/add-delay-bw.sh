ETH=10gp1
DELAY=$1
BW=$2
BURST=400 #$2 #`expr $2 \* 5`
#LAT=$5

sudo modprobe ifb
ip link set dev ifb0 up
sudo tc qdisc add dev ${ETH} ingress
sudo tc filter add dev ${ETH} parent ffff: protocol ip u32 match u32 0 0 flowid 1:1 action mirred egress redirect dev ifb0

sudo tc qdisc add dev ifb0 root handle 1: netem limit 10000000 delay ${DELAY}ms
sudo tc qdisc add dev ifb0 parent 1: handle 2: tbf rate ${BW}mbit burst ${BURST}kbit latency ${DELAY}ms

#sudo tc qdisc add dev ifb0 root handle 1: tbf rate ${BW}mbit burst ${BURST}kbit latency 20ms
#sudo tc qdisc add dev ifb0 parent 1: handle 2: netem delay ${DELAY}ms

echo "Created a" $BW "mbps link with 1BDP buffering and" $DELAY "ms delay."

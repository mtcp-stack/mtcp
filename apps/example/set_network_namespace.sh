# Clean up  
sudo ip netns del ns_client 2>/dev/null
sudo ip link del veth_host 2>/dev/null

# Create namespace + veth pair
sudo ip netns add ns_client
sudo ip link add veth_host type veth peer name veth_client
sudo ip link set veth_client netns ns_client

# Assign IPs
sudo ip addr add 10.200.1.1/24 dev veth_host
sudo ip link set veth_host up
sudo ip netns exec ns_client ip addr add 10.200.1.2/24 dev veth_client
sudo ip netns exec ns_client ip link set veth_client up

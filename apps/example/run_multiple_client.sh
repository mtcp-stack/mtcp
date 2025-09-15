#!/bin/bash

# Run 5 clients in background, each fetching a different file
for i in {1..5}; do
    FILE="${i}"
    (
     echo "[Client $i] Starting with file $FILE ..."
    sudo ip netns exec ns_client ./sample_client "$FILE" &
    )
done

# Wait for all clients to finish
wait
echo "All clients finished."
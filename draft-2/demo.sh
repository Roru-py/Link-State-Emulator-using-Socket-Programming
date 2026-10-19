#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"

g++ -std=c++17 -Wall -Wextra -O2 "$DIR/virtual_node.cpp" -o "$DIR/virtual_node"

IP=$(python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.connect(('8.8.8.8', 80)); print(s.getsockname()[0]); s.close()")

python3 "$DIR/oracle_node.py" "$DIR/oracle-node-config.txt" &
ON_PID=$!
sleep 0.4

"$DIR/virtual_node" "$IP" "$IP" 6001 &
P1=$!
sleep 0.05
"$DIR/virtual_node" "$IP" "$IP" 6002 &
P2=$!
sleep 0.05
"$DIR/virtual_node" "$IP" "$IP" 6003 &
P3=$!
sleep 0.05
"$DIR/virtual_node" "$IP" "$IP" 6004 &
P4=$!
sleep 0.05
"$DIR/virtual_node" "$IP" "$IP" 6005 &
P5=$!

wait $ON_PID $P1 $P2 $P3 $P4 $P5 2>/dev/null


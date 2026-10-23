#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"
CONFIG="${1:-$DIR/config-5node.txt}"

g++ -std=c++17 -Wall -Wextra -O2 "$DIR/virtual_node.cpp" -o "$DIR/virtual_node"

IP=$(python3 -c "import socket; s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); s.connect(('8.8.8.8',80)); print(s.getsockname()[0]); s.close()")

N=$(python3 -c "
import math
tokens=[]
with open('$CONFIG') as f:
    for line in f:
        line=line.split('#')[0].strip()
        if line:
            tokens.extend(line.split())
print(int((1+math.isqrt(1+8*len(tokens)))/2))
")

echo "Config: $CONFIG  |  N=$N nodes  |  ON IP=$IP"

python3 "$DIR/oracle_node.py" "$CONFIG" "$IP" &
ON_PID=$!
sleep 0.5

PIDS=()
for i in $(seq 1 $N); do
    PORT=$((6000 + i))
    "$DIR/virtual_node" "$IP" "$IP" "$PORT" &
    PIDS+=($!)
    sleep 0.05
done

cleanup() {
    kill $ON_PID "${PIDS[@]}" 2>/dev/null
    exit
}
trap cleanup SIGINT SIGTERM

wait

import socket
import struct
import sys
import math
import os
import select


def read_config(filename):
    numbers = []
    with open(filename, "r") as f:
        for line in f:
            clean = line.split("#")[0].strip()
            if clean:
                numbers.extend(clean.split())

    n = int((1 + math.isqrt(1 + 8 * len(numbers))) // 2)
    graph = [[-1] * n for _ in range(n)]
    for i in range(n):
        graph[i][i] = 0

    idx = 0
    for i in range(n):
        for j in range(i + 1, n):
            cost = int(numbers[idx])
            graph[i][j] = cost
            graph[j][i] = cost
            idx += 1

    return n, graph


def send_link_state(n, graph, nodes):
    for i in range(n):
        node_id = chr(ord("A") + i)
        payload = struct.pack(
            "!c4sHi",
            node_id.encode(),
            nodes[node_id]["ip"],
            nodes[node_id]["port"],
            0,
        )

        for j in range(n):
            if graph[i][j] > 0:
                nbr_id = chr(ord("A") + j)
                payload += struct.pack(
                    "!c4sHi",
                    nbr_id.encode(),
                    nodes[nbr_id]["ip"],
                    nodes[nbr_id]["port"],
                    graph[i][j],
                )

        count = len(payload) // 11
        try:
            packet = struct.pack("!B", count) + payload
            nodes[node_id]["sock"].sendall(packet)
        except OSError:
            pass


def recv_bytes(sock, size):
    buf = b""
    while len(buf) < size:
        chunk = sock.recv(size - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


def main():
    if len(sys.argv) != 3:
        print(f"Usage: python3 {sys.argv[0]} <config_file> <ON_IP_ADDRESS>")
        sys.exit(1)

    config_file = sys.argv[1]
    on_ip = sys.argv[2]

    if on_ip.startswith("127.") or on_ip == "0.0.0.0":
        print("Error: Please provide a valid non-loopback LAN IP address")
        sys.exit(1)

    n, graph = read_config(config_file)
    if n < 1 or n > 26:
        print("Error: Config must contain between 1 and 26 nodes")
        sys.exit(1)

    last_modified = os.path.getmtime(config_file)

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((on_ip, 5000))
    server.listen(n)

    print(f"Oracle Node on {on_ip}:5000 waiting for {n} Virtual Nodes...")

    nodes = {}
    for i in range(n):
        conn, addr = server.accept()
        letter = chr(ord("A") + i)
        raw_connect = recv_bytes(conn, 6)
        if not raw_connect:
            print(f"Error receiving CONNECT from node {letter}")
            sys.exit(1)

        node_ip = raw_connect[:4]
        udp_port = struct.unpack("!H", raw_connect[4:6])[0]
        nodes[letter] = {
            "ip": node_ip,
            "port": udp_port,
            "sock": conn,
        }
        print(f"Accepted {addr}. Assigned ID: {letter}")

    print("All nodes connected. Sending initial LINK-STATE tuples...")
    send_link_state(n, graph, nodes)

    client_socks = [nodes[k]["sock"] for k in nodes]

    running = True
    while running:
        try:
            readable, _, _ = select.select(client_socks, [], [], 1.0)
            for s in readable:
                data = s.recv(1024)
                if not data:
                    running = False
                    break

            mtime = os.path.getmtime(config_file)
            if mtime != last_modified:
                last_modified = mtime
                try:
                    new_n, new_graph = read_config(config_file)
                    if new_n == n and new_graph != graph:
                        graph = new_graph
                        print("Config file change detected. Re-sending LINK-STATE tuples...")
                        send_link_state(n, graph, nodes)
                except Exception:
                    pass

        except KeyboardInterrupt:
            break
        except Exception:
            break

    for k in nodes:
        try:
            nodes[k]["sock"].close()
        except OSError:
            pass
    server.close()


if __name__ == "__main__":
    main()

    main()
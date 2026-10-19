import socket
import struct
import sys
import math


def getLanIP():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    except Exception:
        return "127.0.0.1"
    finally:
        s.close()


def parseConfig(filename):
    tokens = []
    with open(filename, "r") as f:
        for line in f:
            cleanLine = line.split("#")[0].strip()
            if cleanLine:
                tokens.extend(cleanLine.split())

    val = 1 + 8 * len(tokens)
    N = int((1 + math.isqrt(val)) / 2)

    matrix = [[-1] * N for _ in range(N)]
    idx = 0
    for i in range(N):
        matrix[i][i] = 0
        for j in range(i + 1, N):
            matrix[i][j] = matrix[j][i] = int(tokens[idx])
            idx += 1

    return N, matrix


def main():
    if len(sys.argv) != 2:
        print(f"Usage: python3 {sys.argv[0]} <config_file>")
        sys.exit(1)

    N, graph = parseConfig(sys.argv[1])
    LanIP = getLanIP()

    serverSock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    serverSock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    serverSock.bind((LanIP, 5000))
    serverSock.listen(N)

    print(f"Oracle Node on {LanIP}:5000 waiting for {N} Virtual Nodes...")

    vnInfo = {}

    for i in range(N):
        clientSock, addr = serverSock.accept()
        char_id = chr(ord("A") + i)

        data = clientSock.recv(6)
        ipBytes = data[:4]
        port = struct.unpack("!H", data[4:6])[0]

        vnInfo[char_id] = {"ip_bytes": ipBytes, "port": port, "sock": clientSock}
        print(f"Accepted {addr}. Assigned ID: {char_id}")

    print("All nodes connected. Sending out LINK-STATE tuples...")

    for i in range(N):
        myChar = chr(ord("A") + i)
        payload = b""

        payload += struct.pack(
            "!c 4s H i",
            myChar.encode("ascii"),
            vnInfo[myChar]["ip_bytes"],
            vnInfo[myChar]["port"],
            0,
        )

        for j in range(N):
            cost = graph[i][j]
            if cost > 0:
                neighbourChar = chr(ord("A") + j)
                payload += struct.pack(
                    "!c 4s H i",
                    neighbourChar.encode("ascii"),
                    vnInfo[neighbourChar]["ip_bytes"],
                    vnInfo[neighbourChar]["port"],
                    cost,
                )

        vnInfo[myChar]["sock"].sendall(payload)

    import time
    time.sleep(0.5)

    for i in range(N):
        myChar = chr(ord("A") + i)
        vnInfo[myChar]["sock"].close()
    serverSock.close()


if __name__ == "__main__":
    main()

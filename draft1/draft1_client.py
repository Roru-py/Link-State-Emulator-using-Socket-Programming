import socket

SERVER_IP = "172.20.10.3" 
SERVER_PORT = 4999

#abhi keliye useless
MY_IP = "172.20.10.3"
MY_UDP_PORT = 2000

#this is to setup udp port as receiver for peer connections in the future after we do lsp (no need now)
udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
udp_sock.bind((MY_IP, MY_UDP_PORT))


tcp_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
tcp_client.connect((SERVER_IP, SERVER_PORT))

print("connection established to server")
message = f"my ip and udp port for sending to peers: {MY_IP} and {MY_UDP_PORT}"

tcp_client.sendall(message.encode())
print("sent:", message)
reply = tcp_client.recv(1024).decode()
print("reply:", reply)

tcp_client.close()
#do something here in draft-2 to run lsp and communicate with peer
udp_sock.close()
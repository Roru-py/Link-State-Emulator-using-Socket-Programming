#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define ON_PORT 5000
#define MAX_NODES 26
#define INF_COST -1

#pragma pack(push, 1)
struct ConnectMsg {
  uint32_t ip;
  uint16_t port;
};
struct LSTuple {
  char node;
  uint32_t ip;
  uint16_t port;
  int32_t cost;
};
#pragma pack(pop)

int main(int argc, char *argv[]) {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0]
              << " <ON_IP_ADDRESS> <VN_IP_ADDRESS> <VN_UDP_PORT>\n";
    return 1;
  }

  uint32_t vnIP = inet_addr(argv[2]);
  uint16_t vnPort = htons(std::stoi(argv[3]));

  int tcpSock = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in on_addr;
  on_addr.sin_family = AF_INET;
  on_addr.sin_port = htons(5000);
  on_addr.sin_addr.s_addr = inet_addr(argv[1]);

  if (connect(tcpSock, (struct sockaddr *)&on_addr, sizeof(on_addr)) < 0) {
    perror("TCP connection to ON failed");
    return 1;
  }

  ConnectMsg cmsg = {vnIP, vnPort};
  send(tcpSock, &cmsg, sizeof(cmsg), 0);

  std::cout << "Connected to ON. Waiting for LS..." << std::endl;

  while (true) {
    char buf[2048];
    int bytes = recv(tcpSock, buf, sizeof(buf), 0);
    if (bytes <= 0) {
      std::cout << "\nOracle Node disconnected." << std::endl;
      break;
    }

    std::string print_out = "";
    for (int i = 0; i < bytes / (int)sizeof(LSTuple); i++) {
      LSTuple t = ((LSTuple *)buf)[i];
      int cost = ntohl(t.cost);

      if (cost == 0) {
        print_out = std::string(1, t.node) + "=0, " + print_out;
      } else {
        print_out += std::string(1, t.node) + "=" + std::to_string(cost) + ", ";
      }
    }

    if (print_out.length() > 2)
      print_out.erase(print_out.length() - 2);
    std::cout << "Received " << print_out << std::endl;
  }
  close(tcpSock);
  return 0;
}

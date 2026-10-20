#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define ON_PORT 5000
#define MAX_NODES 26
#define INF_COST 1000000000

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
struct LSPHeader {
  char origin;
  uint32_t seq;
  uint8_t num_links;
};
struct LSPLink {
  char neighbor;
  int32_t cost;
};
#pragma pack(pop)

struct Neighbor {
  char node;
  uint32_t ip;
  uint16_t port;
  int cost;
};

void sendLSP(int udpSock, char myNode, uint32_t mySeq, const std::vector<Neighbor> &neighbors, uint32_t excludeIP = 0, uint16_t excludePort = 0) {
  std::vector<uint8_t> packet(sizeof(LSPHeader) + neighbors.size() * sizeof(LSPLink));
  LSPHeader *hdr = (LSPHeader *)packet.data();
  hdr->origin = myNode;
  hdr->seq = htonl(mySeq);
  hdr->num_links = (uint8_t)neighbors.size();

  LSPLink *links = (LSPLink *)(packet.data() + sizeof(LSPHeader));
  for (size_t i = 0; i < neighbors.size(); ++i) {
    links[i].neighbor = neighbors[i].node;
    links[i].cost = htonl(neighbors[i].cost);
  }

  for (const auto &nbr : neighbors) {
    if (excludeIP != 0 && nbr.ip == excludeIP && nbr.port == excludePort) {
      continue;
    }
    sockaddr_in destAddr{};
    destAddr.sin_family = AF_INET;
    destAddr.sin_addr.s_addr = nbr.ip;
    destAddr.sin_port = htons(nbr.port);
    sendto(udpSock, packet.data(), packet.size(), 0, (sockaddr *)&destAddr, sizeof(destAddr));
    sendto(udpSock, packet.data(), packet.size(), 0, (sockaddr *)&destAddr, sizeof(destAddr));
  }
}

void forwardLSP(int udpSock, const uint8_t *packet, size_t packetLen, const std::vector<Neighbor> &neighbors, uint32_t fromIP, uint16_t fromPort) {
  for (const auto &nbr : neighbors) {
    if (nbr.ip == fromIP && nbr.port == fromPort) {
      continue;
    }
    sockaddr_in destAddr{};
    destAddr.sin_family = AF_INET;
    destAddr.sin_addr.s_addr = nbr.ip;
    destAddr.sin_port = htons(nbr.port);
    sendto(udpSock, packet, packetLen, 0, (sockaddr *)&destAddr, sizeof(destAddr));
    sendto(udpSock, packet, packetLen, 0, (sockaddr *)&destAddr, sizeof(destAddr));
  }
}

void runDijkstra(char myNode, int graph[MAX_NODES][MAX_NODES], const bool knownNodes[MAX_NODES]) {
  int src = myNode - 'A';
  int dist[MAX_NODES];
  bool visited[MAX_NODES];

  for (int i = 0; i < MAX_NODES; ++i) {
    dist[i] = INF_COST;
    visited[i] = false;
  }
  dist[src] = 0;

  for (int count = 0; count < MAX_NODES; ++count) {
    int u = -1;
    int min_d = INF_COST;
    for (int i = 0; i < MAX_NODES; ++i) {
      if (knownNodes[i] && !visited[i] && dist[i] < min_d) {
        min_d = dist[i];
        u = i;
      }
    }
    if (u == -1) break;
    visited[u] = true;

    for (int v = 0; v < MAX_NODES; ++v) {
      if (knownNodes[v] && !visited[v] && graph[u][v] > 0) {
        if (dist[u] + graph[u][v] < dist[v]) {
          dist[v] = dist[u] + graph[u][v];
        }
      }
    }
  }

  std::string dijkstraOut = "";
  for (int i = 0; i < MAX_NODES; ++i) {
    if (knownNodes[i] && i != src) {
      char target = (char)('A' + i);
      dijkstraOut += std::string(1, target) + "=" + (dist[i] == INF_COST ? "INF" : std::to_string(dist[i])) + ", ";
    }
  }
  if (dijkstraOut.length() > 2) dijkstraOut.erase(dijkstraOut.length() - 2);
  std::cout << "Shortest Paths from " << myNode << ": " << dijkstraOut << std::endl;
}

void printGraph(char myNode, int graph[MAX_NODES][MAX_NODES], const bool knownNodes[MAX_NODES]) {
  std::vector<char> nodes;
  for (int i = 0; i < MAX_NODES; ++i) {
    if (knownNodes[i]) nodes.push_back((char)('A' + i));
  }
  if (nodes.size() <= 1) return;

  std::cout << "\n--- Known Graph (Node " << myNode << ") ---" << std::endl;
  for (size_t i = 0; i < nodes.size() - 1; ++i) {
    for (size_t t = 0; t < i; ++t) std::cout << "\t";
    for (size_t j = i + 1; j < nodes.size(); ++j) {
      int u = nodes[i] - 'A';
      int v = nodes[j] - 'A';
      int cost = -1;
      if (graph[u][v] != -1) cost = graph[u][v];
      else if (graph[v][u] != -1) cost = graph[v][u];
      std::cout << cost;
      if (j + 1 < nodes.size()) std::cout << "\t";
    }
    std::cout << std::endl;
  }
  runDijkstra(myNode, graph, knownNodes);
}

int main(int argc, char *argv[]) {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0]
              << " <ON_IP_ADDRESS> <VN_IP_ADDRESS> <VN_UDP_PORT>\n";
    return 1;
  }

  uint32_t vnIP = inet_addr(argv[2]);
  uint16_t vnPort = (uint16_t)std::stoi(argv[3]);

  int udpSock = socket(AF_INET, SOCK_DGRAM, 0);
  if (udpSock < 0) {
    perror("UDP socket creation failed");
    return 1;
  }

  sockaddr_in udpAddr{};
  udpAddr.sin_family = AF_INET;
  udpAddr.sin_port = htons(vnPort);
  udpAddr.sin_addr.s_addr = vnIP;
  if (bind(udpSock, (sockaddr *)&udpAddr, sizeof(udpAddr)) < 0) {
    perror("UDP bind failed");
    close(udpSock);
    return 1;
  }

  int tcpSock = socket(AF_INET, SOCK_STREAM, 0);
  if (tcpSock < 0) {
    perror("TCP socket creation failed");
    close(udpSock);
    return 1;
  }

  struct sockaddr_in on_addr{};
  on_addr.sin_family = AF_INET;
  on_addr.sin_port = htons(5000);
  on_addr.sin_addr.s_addr = inet_addr(argv[1]);

  if (connect(tcpSock, (struct sockaddr *)&on_addr, sizeof(on_addr)) < 0) {
    perror("TCP connection to ON failed");
    close(udpSock);
    close(tcpSock);
    return 1;
  }

  ConnectMsg cmsg = {vnIP, htons(vnPort)};
  send(tcpSock, &cmsg, sizeof(cmsg), 0);

  std::cout << "Connected to ON. Waiting for LS..." << std::endl;

  char myNode = '\0';
  uint32_t mySeq = 0;
  std::vector<Neighbor> neighbors;
  int graph[MAX_NODES][MAX_NODES];
  bool knownNodes[MAX_NODES];
  uint32_t maxSeq[MAX_NODES];

  for (int i = 0; i < MAX_NODES; ++i) {
    knownNodes[i] = false;
    maxSeq[i] = 0;
    for (int j = 0; j < MAX_NODES; ++j) {
      graph[i][j] = (i == j ? 0 : -1);
    }
  }

  time_t lastLSPBroadcast = time(NULL);
  time_t lastGraphPrint = time(NULL);

  while (true) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(tcpSock, &readfds);
    FD_SET(udpSock, &readfds);
    int maxfd = (tcpSock > udpSock ? tcpSock : udpSock);

    timeval tv{1, 0};
    int sel = select(maxfd + 1, &readfds, NULL, NULL, &tv);

    if (sel > 0) {
      if (FD_ISSET(tcpSock, &readfds)) {
        char buf[2048];
        int bytes = recv(tcpSock, buf, sizeof(buf), 0);
        if (bytes <= 0) {
          std::cout << "\nOracle Node disconnected." << std::endl;
          break;
        }

        neighbors.clear();
        std::string print_out = "";
        for (int i = 0; i < bytes / (int)sizeof(LSTuple); i++) {
          LSTuple t = ((LSTuple *)buf)[i];
          int cost = ntohl(t.cost);

          if (cost == 0) {
            myNode = t.node;
            knownNodes[myNode - 'A'] = true;
            print_out = std::string(1, t.node) + "=0, " + print_out;
          } else {
            neighbors.push_back({t.node, t.ip, ntohs(t.port), cost});
            knownNodes[t.node - 'A'] = true;
            graph[myNode - 'A'][t.node - 'A'] = cost;
            graph[t.node - 'A'][myNode - 'A'] = cost;
            print_out += std::string(1, t.node) + "=" + std::to_string(cost) + ", ";
          }
        }

        if (print_out.length() > 2) print_out.erase(print_out.length() - 2);
        std::cout << "Received " << print_out << std::endl;

        if (myNode != '\0') {
          mySeq++;
          maxSeq[myNode - 'A'] = mySeq;
          sendLSP(udpSock, myNode, mySeq, neighbors);
          lastLSPBroadcast = time(NULL);
        }
      }

      if (FD_ISSET(udpSock, &readfds)) {
        uint8_t udpBuf[2048];
        sockaddr_in fromAddr{};
        socklen_t fromLen = sizeof(fromAddr);
        int bytes = recvfrom(udpSock, udpBuf, sizeof(udpBuf), 0, (sockaddr *)&fromAddr, &fromLen);

        if (bytes >= (int)sizeof(LSPHeader)) {
          LSPHeader *hdr = (LSPHeader *)udpBuf;
          char origin = hdr->origin;
          uint32_t seq = ntohl(hdr->seq);
          uint8_t num_links = hdr->num_links;

          if (bytes >= (int)(sizeof(LSPHeader) + num_links * sizeof(LSPLink))) {
            int origIdx = origin - 'A';
            if (origin != myNode && origIdx >= 0 && origIdx < MAX_NODES) {
              if (seq > maxSeq[origIdx]) {
                maxSeq[origIdx] = seq;
                knownNodes[origIdx] = true;

                LSPLink *links = (LSPLink *)(udpBuf + sizeof(LSPHeader));
                for (int i = 0; i < MAX_NODES; ++i) {
                  if (i != origIdx) graph[origIdx][i] = -1;
                }
                for (int i = 0; i < num_links; ++i) {
                  char nbrChar = links[i].neighbor;
                  int nbrIdx = nbrChar - 'A';
                  int cost = ntohl(links[i].cost);
                  if (nbrIdx >= 0 && nbrIdx < MAX_NODES) {
                    knownNodes[nbrIdx] = true;
                    graph[origIdx][nbrIdx] = cost;
                    graph[nbrIdx][origIdx] = cost;
                  }
                }

                forwardLSP(udpSock, udpBuf, bytes, neighbors, fromAddr.sin_addr.s_addr, ntohs(fromAddr.sin_port));
              }
            }
          }
        }
      }
    }

    time_t now = time(NULL);
    if (myNode != '\0') {
      if (now - lastLSPBroadcast >= 15) {
        lastLSPBroadcast = now;
        mySeq++;
        maxSeq[myNode - 'A'] = mySeq;
        sendLSP(udpSock, myNode, mySeq, neighbors);
      }
      if (now - lastGraphPrint >= 10) {
        lastGraphPrint = now;
        printGraph(myNode, graph, knownNodes);
      }
    }
  }

  close(tcpSock);
  close(udpSock);
  return 0;
}

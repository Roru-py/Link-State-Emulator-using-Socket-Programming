#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>

using namespace std;

const int MAX_NODES = 26;
const int INF = 1000000000;

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
    uint8_t numLinks;
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

char my_node = 0;
uint32_t my_seq = 0;
vector<Neighbor> neighbors;
int graph[MAX_NODES][MAX_NODES];
bool known[MAX_NODES];
uint32_t max_seq[MAX_NODES];

bool recv_all(int fd, void *buf, size_t total) {
    char *ptr = (char *)buf;
    size_t received = 0;
    while (received < total) {
        ssize_t n = recv(fd, ptr + received, total - received, 0);
        if (n <= 0) return false;
        received += n;
    }
    return true;
}

bool send_all(int fd, const void *buf, size_t total) {
    const char *ptr = (const char *)buf;
    size_t sent = 0;
    while (sent < total) {
        ssize_t n = send(fd, ptr + sent, total - sent, 0);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

void flood_lsp(int udp_sock, const uint8_t *packet, size_t len, uint32_t skip_ip = 0, uint16_t skip_port = 0) {
    for (size_t i = 0; i < neighbors.size(); i++) {
        if (neighbors[i].ip == skip_ip && neighbors[i].port == skip_port) {
            continue;
        }

        sockaddr_in dest_addr{};
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_addr.s_addr = neighbors[i].ip;
        dest_addr.sin_port = htons(neighbors[i].port);

        sendto(udp_sock, packet, len, 0, (sockaddr *)&dest_addr, sizeof(dest_addr));
        sendto(udp_sock, packet, len, 0, (sockaddr *)&dest_addr, sizeof(dest_addr));
    }
}

void send_own_lsp(int udp_sock) {
    my_seq++;
    max_seq[my_node - 'A'] = my_seq;

    size_t packet_size = sizeof(LSPHeader) + neighbors.size() * sizeof(LSPLink);
    vector<uint8_t> buffer(packet_size);

    LSPHeader *hdr = (LSPHeader *)buffer.data();
    hdr->origin = my_node;
    hdr->seq = htonl(my_seq);
    hdr->numLinks = (uint8_t)neighbors.size();

    LSPLink *link_data = (LSPLink *)(buffer.data() + sizeof(LSPHeader));
    for (size_t i = 0; i < neighbors.size(); i++) {
        link_data[i].neighbor = neighbors[i].node;
        link_data[i].cost = htonl(neighbors[i].cost);
    }

    flood_lsp(udp_sock, buffer.data(), buffer.size());
}

void run_dijkstra() {
    int src = my_node - 'A';
    int dist[MAX_NODES];
    bool visited[MAX_NODES];

    for (int i = 0; i < MAX_NODES; i++) {
        dist[i] = INF;
        visited[i] = false;
    }
    dist[src] = 0;

    for (int step = 0; step < MAX_NODES; step++) {
        int u = -1;
        int min_val = INF;
        for (int i = 0; i < MAX_NODES; i++) {
            if (known[i] && !visited[i] && dist[i] < min_val) {
                min_val = dist[i];
                u = i;
            }
        }
        if (u == -1) break;
        visited[u] = true;

        for (int v = 0; v < MAX_NODES; v++) {
            if (known[v] && !visited[v] && graph[u][v] > 0) {
                if (dist[u] + graph[u][v] < dist[v]) {
                    dist[v] = dist[u] + graph[u][v];
                }
            }
        }
    }

    string res = "";
    for (int i = 0; i < MAX_NODES; i++) {
        if (known[i] && i != src) {
            char target = (char)('A' + i);
            res += string(1, target) + "=" + (dist[i] == INF ? "INF" : to_string(dist[i])) + ", ";
        }
    }
    if (res.size() > 2) res.erase(res.size() - 2);
    cout << "Shortest Paths from " << my_node << ": " << res << endl;
}

void print_adj_list() {
    vector<int> node_indices;
    for (int i = 0; i < MAX_NODES; i++) {
        if (known[i]) {
            node_indices.push_back(i);
        }
    }
    if (node_indices.size() <= 1) return;

    cout << "\n--- Known Graph (Node " << my_node << ") ---" << endl;
    for (size_t i = 0; i < node_indices.size(); i++) {
        int u = node_indices[i];
        cout << (char)('A' + u) << ": ";
        bool first = true;
        for (size_t j = 0; j < node_indices.size(); j++) {
            int v = node_indices[j];
            if (u != v && graph[u][v] > 0) {
                if (!first) cout << ", ";
                cout << (char)('A' + v) << "=" << graph[u][v];
                first = false;
            }
        }
        cout << endl;
    }
    run_dijkstra();
}

bool handle_link_state(int tcp_sock, int udp_sock) {
    uint8_t count = 0;
    if (!recv_all(tcp_sock, &count, 1)) {
        cout << "\nOracle Node disconnected." << endl;
        return false;
    }

    vector<LSTuple> tuples(count);
    if (!recv_all(tcp_sock, tuples.data(), count * sizeof(LSTuple))) {
        cout << "\nIncomplete LINK-STATE message from Oracle Node." << endl;
        return false;
    }

    neighbors.clear();
    if (my_node != 0) {
        cout << "\nConfig change received at Node " << my_node << "!" << endl;
        int me = my_node - 'A';
        for (int i = 0; i < MAX_NODES; i++) {
            graph[me][i] = (i == me ? 0 : -1);
            graph[i][me] = (i == me ? 0 : -1);
        }
    }

    string out = "";
    for (size_t i = 0; i < tuples.size(); i++) {
        int cost = ntohl(tuples[i].cost);
        if (cost == 0) {
            my_node = tuples[i].node;
            known[my_node - 'A'] = true;
            out = string(1, tuples[i].node) + "=0, " + out;
        } else {
            neighbors.push_back({tuples[i].node, tuples[i].ip, ntohs(tuples[i].port), cost});
            known[tuples[i].node - 'A'] = true;
            graph[my_node - 'A'][tuples[i].node - 'A'] = cost;
            graph[tuples[i].node - 'A'][my_node - 'A'] = cost;
            out += string(1, tuples[i].node) + "=" + to_string(cost) + ", ";
        }
    }

    if (out.size() > 2) out.erase(out.size() - 2);
    cout << "Received " << out << endl;

    if (my_node != 0) {
        send_own_lsp(udp_sock);
    }
    return true;
}

void handle_incoming_lsp(int udp_sock) {
    uint8_t buf[2048];
    sockaddr_in sender_addr{};
    socklen_t addr_len = sizeof(sender_addr);

    int bytes = recvfrom(udp_sock, buf, sizeof(buf), 0, (sockaddr *)&sender_addr, &addr_len);
    if (bytes < (int)sizeof(LSPHeader)) return;

    LSPHeader *hdr = (LSPHeader *)buf;
    char origin = hdr->origin;
    int origin_idx = origin - 'A';
    uint32_t seq = ntohl(hdr->seq);
    int num_links = hdr->numLinks;

    if (bytes < (int)(sizeof(LSPHeader) + num_links * sizeof(LSPLink))) return;
    if (origin == my_node || origin_idx < 0 || origin_idx >= MAX_NODES) return;
    if (seq <= max_seq[origin_idx]) return;

    max_seq[origin_idx] = seq;
    known[origin_idx] = true;

    for (int i = 0; i < MAX_NODES; i++) {
        if (i != origin_idx) {
            graph[origin_idx][i] = -1;
            graph[i][origin_idx] = -1;
        }
    }

    LSPLink *link_arr = (LSPLink *)(buf + sizeof(LSPHeader));
    for (int i = 0; i < num_links; i++) {
        int nb_idx = link_arr[i].neighbor - 'A';
        int cost = ntohl(link_arr[i].cost);
        if (nb_idx >= 0 && nb_idx < MAX_NODES) {
            known[nb_idx] = true;
            graph[origin_idx][nb_idx] = cost;
            graph[nb_idx][origin_idx] = cost;
        }
    }

    flood_lsp(udp_sock, buf, bytes, sender_addr.sin_addr.s_addr, ntohs(sender_addr.sin_port));
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " <ON_IP_ADDRESS> <VN_IP_ADDRESS> <VN_UDP_PORT>\n";
        return 1;
    }

    uint32_t vn_ip = inet_addr(argv[2]);
    uint16_t vn_port = (uint16_t)stoi(argv[3]);

    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) {
        perror("UDP socket creation failed");
        return 1;
    }

    sockaddr_in udp_addr{};
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_port = htons(vn_port);
    udp_addr.sin_addr.s_addr = vn_ip;
    if (bind(udp_sock, (sockaddr *)&udp_addr, sizeof(udp_addr)) < 0) {
        perror("UDP bind failed");
        close(udp_sock);
        return 1;
    }

    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock < 0) {
        perror("TCP socket creation failed");
        close(udp_sock);
        return 1;
    }

    sockaddr_in on_addr{};
    on_addr.sin_family = AF_INET;
    on_addr.sin_port = htons(5000);
    on_addr.sin_addr.s_addr = inet_addr(argv[1]);

    if (connect(tcp_sock, (sockaddr *)&on_addr, sizeof(on_addr)) < 0) {
        perror("TCP connection to ON failed");
        close(udp_sock);
        close(tcp_sock);
        return 1;
    }

    ConnectMsg cmsg = {vn_ip, htons(vn_port)};
    if (!send_all(tcp_sock, &cmsg, sizeof(cmsg))) {
        cerr << "CONNECT message send failed\n";
        close(udp_sock);
        close(tcp_sock);
        return 1;
    }

    cout << "Connected to ON. Waiting for LS..." << endl;

    for (int i = 0; i < MAX_NODES; i++) {
        known[i] = false;
        max_seq[i] = 0;
        for (int j = 0; j < MAX_NODES; j++) {
            graph[i][j] = (i == j ? 0 : -1);
        }
    }

    time_t last_lsp = time(NULL);
    time_t last_print = time(NULL);

    while (true) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(tcp_sock, &read_fds);
        FD_SET(udp_sock, &read_fds);
        int max_fd = (tcp_sock > udp_sock ? tcp_sock : udp_sock);

        timeval tv{1, 0};
        int ready = select(max_fd + 1, &read_fds, NULL, NULL, &tv);

        if (ready > 0) {
            if (FD_ISSET(tcp_sock, &read_fds)) {
                if (!handle_link_state(tcp_sock, udp_sock)) break;
                last_lsp = time(NULL);
            }
            if (FD_ISSET(udp_sock, &read_fds)) {
                handle_incoming_lsp(udp_sock);
            }
        }

        time_t now = time(NULL);
        if (my_node != 0) {
            if (now - last_lsp >= 15) {
                last_lsp = now;
                send_own_lsp(udp_sock);
            }
            if (now - last_print >= 10) {
                last_print = now;
                print_adj_list();
            }
        }
    }

    close(tcp_sock);
    close(udp_sock);
    return 0;
}

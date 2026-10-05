#include <bits/stdc++.h>
#include <netinet/in.h>
using namespace std;

int main() {
    char message[16]{};
    string msg = "hello\n";
    int s = socket(PF_INET, SOCK_DGRAM, 0);

    sockaddr_in addr{
        .sin_family = AF_INET,
        .sin_port = htons(5000)
    };

    bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    sendto(s, msg.data(), msg.size(), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    int n = recvfrom(s, message, sizeof(message), 0, nullptr, nullptr);
    cout.write(message, n);

    close(s);
}
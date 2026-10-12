#include <bits/stdc++.h>
#include <netinet/in.h>
using namespace std;

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in sadd{};
    sadd.sin_family = AF_INET;
    int PORT = 4999;
    sadd.sin_port = htons(PORT);
    sadd.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd,(sockaddr*)&sadd,sizeof(sadd));
    listen(server_fd, 6); //we can hv this many comms simultaneously 

    printf("server listening on port %d\n",PORT);

    sockaddr_in cadd{};
    socklen_t clen = sizeof(cadd);
    int client_fd = accept(server_fd,(sockaddr*)&cadd,&clen);

    cout << "client connected" << endl;

    char msgstr[1024] = {};
    ssize_t n = recv(client_fd,msgstr,sizeof(msgstr)-1,0);

    if(n>0) { 
        msgstr[n] = '\0';
        cout << "msg from client " << msgstr << '\n';

        string reply = "ack from server";
        send(client_fd,reply.data(),reply.size(),0);
    }

    close(client_fd);
    close(server_fd);
}